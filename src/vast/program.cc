#include <vast/program.h>

#include <cstdlib>
#include <iostream>
#include <boost/exception/diagnostic_information.hpp>
#include <vast/exception.h>
#include <vast/comm/broccoli.h>
#include <vast/detail/cppa_type_info.h>
#include <vast/fs/path.h>
#include <vast/fs/operations.h>
#include <vast/ingest/ingestor.h>
#include <vast/meta/schema_manager.h>
#include <vast/query/client.h>
#include <vast/query/search.h>
#include <vast/store/archive.h>
#include <vast/util/logger.h>
#include <config.h>


#ifdef USE_PERFTOOLS
#include <google/profiler.h>
#include <google/heap-profiler.h>
#endif

namespace vast {

/// Declaration of global (extern) variables.
namespace util {
logger* LOGGER;
}

program::program()
  : terminating_(false)
  , return_(EXIT_SUCCESS)
{
}

program::~program()
{
}

bool program::init(std::string const& filename)
{
  try
  {
    config_.load(filename);
    do_init();
    return true;
  }
  catch (boost::program_options::unknown_option const& e)
  {
    std::cerr << e.what();
  }

  return false;
}

bool program::init(int argc, char *argv[])
{
  try
  {
    config_.load(argc, argv);

    if (argc < 2 || config_.check("help") || config_.check("advanced"))
    {
      config_.print(std::cerr, config_.check("advanced"));
      return false;
    }

    do_init();
    return true;
  }
  catch (config_exception const& e)
  {
    std::cerr << e.what() << std::endl;
  }
  catch (boost::program_options::unknown_option const& e)
  {
    std::cerr << e.what() << ", try -h or --help" << std::endl;
  }
  catch (boost::exception const& e)
  {
    std::cerr << boost::diagnostic_information(e);
  }

  return false;
}

void program::start()
{
  using namespace cppa;
  detail::cppa_announce_types();

  try
  {
    auto log_dir = config_.get<fs::path>("log-dir");
    if (! fs::exists(log_dir))
      fs::mkdir(log_dir);

#ifdef USE_PERFTOOLS
    if (config_.check("perftools-heap"))
    {
      LOG(info, core) << "starting perftools CPU profiler";
      HeapProfilerStart((log_dir / "heap.profile").string().data());
    }
    if (config_.check("perftools-cpu"))
    {
      LOG(info, core) << "starting perftools heap profiler";
      ProfilerStart((log_dir / "cpu.profile").string().data());
    }
#endif

    if (config_.check("profile"))
    {
      LOG(verbose, core) << "spawning profiler";
      auto const& filename = log_dir / "profiler.log";
      auto ms = config_.get<unsigned>("profile-interval");
      profiler_ = spawn<util::profiler>(filename.string(),
                                        std::chrono::milliseconds(ms));
      send(profiler_, atom("run"));
    }

    comm::broccoli::init(config_.check("broccoli-messages"),
                         config_.check("broccoli-calltrace"));

    LOG(verbose, meta) << "spawning schema manager";
    schema_manager_ = spawn<meta::schema_manager>();
    if (config_.check("schema"))
    {
      send(schema_manager_, atom("load"), config_.get<std::string>("schema"));

      if (config_.check("print-schema"))
      {
        send(schema_manager_, atom("print"));
        receive(
            on(atom("schema"), arg_match) >> [](std::string const& schema)
            {
              std::cout << schema << std::endl;
            });

        return;
      }
    }

    if (config_.check("comp-archive"))
    {
      LOG(verbose, store) << "spawning archive";
      archive_ = spawn<store::archive>(
          (config_.get<fs::path>("vast-dir") / "archive").string(),
          config_.get<size_t>("archive.max-events-per-chunk"),
          config_.get<size_t>("archive.max-segment-size") * 1000,
          config_.get<size_t>("archive.max-segments"));
    }

    if (config_.check("comp-ingestor"))
    {
      LOG(verbose, store) << "spawning ingestor";
      ingestor_ = spawn<ingest::ingestor>(archive_);
      send(ingestor_,
           atom("initialize"),
           config_.get<std::string>("ingestor.host"),
           config_.get<unsigned>("ingestor.port"));

      if (config_.check("ingestor.events"))
      {
        auto events = config_.get<std::vector<std::string>>("ingestor.events");
        for (auto& event : events)
          send(ingestor_, atom("subscribe"), event);
      }

      if (config_.check("ingestor.file"))
      {
        auto files = config_.get<std::vector<std::string>>("ingestor.file");
        for (auto& file : files)
        {
          LOG(info, core) << "ingesting " << file;
          send(ingestor_, atom("read_file"), file);
        }
      }
    }

    if (config_.check("comp-search"))
    {
      LOG(verbose, store) << "spawning search";
      search_ = spawn<query::search>(archive_);

      LOG(verbose, store) << "publishing search at "
          << config_.get<std::string>("search.host") << ":"
          << config_.get<unsigned>("search.port");
      send(search_,
           atom("publish"),
           config_.get<std::string>("search.host"),
           config_.get<unsigned>("search.port"));
    }
    else
    {
      LOG(verbose, store) << "connecting to search at "
          << config_.get<std::string>("search.host") << ":"
          << config_.get<unsigned>("search.port");
      search_ = remote_actor(
          config_.get<std::string>("search.host"),
          config_.get<unsigned>("search.port"));
    }

    if (config_.check("query"))
    {
      LOG(verbose, store) << "spawning query client with batch size "
          << config_.get<unsigned>("client.batch-size");

      query_client_ = spawn<query::client>(
          search_,
          config_.get<unsigned>("client.batch-size"));

      send(query_client_,
           atom("query"),
           atom("create"),
           config_.get<std::string>("query"));
    }

    await_all_others_done();
  }
  catch (...)
  {
    LOG(fatal, core)
      << "exception details:\n"
      << boost::current_exception_diagnostic_information();

    return_ = EXIT_FAILURE;
  }
}

void program::stop()
{
  using namespace cppa;

  if (terminating_)
  {
    return_ = EXIT_FAILURE;
    return;
  }

  terminating_ = true;

  auto shutdown = make_any_tuple(atom("shutdown"));

  if (config_.check("query"))
    query_client_ << shutdown;

  if (config_.check("comp-search"))
    search_ << shutdown;

  if (config_.check("comp-ingestor"))
    ingestor_ << shutdown;

  if (config_.check("comp-archive"))
    archive_ << shutdown;

  schema_manager_ << shutdown;

  if (config_.check("profile"))
    profiler_ << shutdown;

#ifdef USE_PERFTOOLS
  if (config_.check("perftools-cpu"))
  {
    LOG(info, core) << "stopping perftools CPU profiler";
    ProfilerStop();
  }
  if (config_.check("perftools-heap") && ::IsHeapProfilerRunning())
  {
    LOG(info, core) << "stopping perftools heap profiler";
    HeapProfilerDump("cleanup");
    HeapProfilerStop();
  }
#endif

  return_ = EXIT_SUCCESS;
}

int program::end()
{
  switch (return_)
  {
    case EXIT_SUCCESS:
      LOG(info, core) << "VAST terminated cleanly";
      break;

    case EXIT_FAILURE:
      LOG(info, core) << "VAST terminated with errors";
      break;

    default:
      assert(! "invalid return code");
  }

  return return_;
}

void program::do_init()
{
  auto vast_dir = config_.get<fs::path>("vast-dir");
  if (! fs::exists(vast_dir))
    fs::mkdir(vast_dir);

  util::LOGGER = new util::logger(
      static_cast<util::logger::level>(config_.get<int>("console-verbosity")),
      static_cast<util::logger::level>(config_.get<int>("logfile-verbosity")),
      config_.get<fs::path>("log-dir") / "vast.log");

  LOG(verbose, core) << " _   _____   __________";
  LOG(verbose, core) << "| | / / _ | / __/_  __/";
  LOG(verbose, core) << "| |/ / __ |_\\ \\  / / ";
  LOG(verbose, core) << "|___/_/ |_/___/ /_/  " << VAST_VERSION;
  LOG(verbose, core) << "";
}

} // namespace vast
