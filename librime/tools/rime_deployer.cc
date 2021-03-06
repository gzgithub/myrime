//
// Copyleft 2012 RIME Developers
// License: GPLv3
//
// 2012-07-07 GONG Chen <chen.sst@gmail.com>
//
#include <iostream>
#include <string>
#include <rime/config.h>
#include <rime/deployer.h>
#include <rime/service.h>
#include <rime/setup.h>
#include <rime/lever/deployment_tasks.h>

int add_schema(int count, char* schemas[]) {
  rime::Config config;
  if (!config.LoadFromFile("default.custom.yaml")) {
    LOG(INFO) << "creating new file 'default.custom.yaml'.";
  }
  rime::ConfigMapEntryRef schema_list(config["patch"]["schema_list"]);
  for (int i = 0; i < count; ++i) {
    if (!schemas[i])
      return 1;
    std::string new_schema_id(schemas[i]);
    bool already_there = false;
    for (size_t j = 0; j < schema_list.size(); ++j) {
      if (!schema_list[j].HasKey("schema"))
        continue;
      std::string schema_id(schema_list[j]["schema"].ToString());
      if (schema_id == new_schema_id) {
        already_there = true;
        break;
      }
    }
    if (already_there) continue;
    schema_list[schema_list.size()]["schema"] = new_schema_id;
    LOG(INFO) << "added schema: " << new_schema_id;
  }
  if (!config.SaveToFile("default.custom.yaml")) {
    LOG(ERROR) << "failed to save schema list.";
    return 1;
  }
  return 0;
}

int set_active_schema(const std::string& schema_id) {
  rime::Config config;
  if (!config.LoadFromFile("user.yaml")) {
    LOG(INFO) << "creating new file 'user.yaml'.";
  }
  config["var"]["previously_selected_schema"] = schema_id;
  if (!config.SaveToFile("user.yaml")) {
    LOG(ERROR) << "failed to set active schema: " << schema_id;
    return 1;
  }
  return 0;
}

static void configure_deployer(rime::Deployer* deployer,
                               int argc, char* argv[]) {
  if (argc > 0) {
    deployer->user_data_dir = argv[0];
    if (argc > 1) {
      deployer->shared_data_dir = argv[1];
    }
    else {
      deployer->shared_data_dir = argv[0];
    }
  }
}

int main(int argc, char* argv[]) {
  rime::SetupLogging("rime.tools");

  if (argc == 1) {
    std::cout << "options:" << std::endl
              << "\t--build [dest_dir [shared_data_dir]]" << std::endl
              << "\t--add-schema schema_id [...]" << std::endl
              << "\t--set-active-schema schema_id" << std::endl
              << "\t--compile x.schema.yaml [dest_dir [shared_data_dir]]" << std::endl
        ;
    return 0;
  }

  std::string option;
  if (argc >= 2) option = argv[1];
  // shift
  argc -= 2, argv += 2;

  if (argc >= 0 && argc <= 2 && option == "--build") {
    rime::Deployer& deployer(rime::Service::instance().deployer());
    configure_deployer(&deployer, argc, argv);
    rime::WorkspaceUpdate update;
    return update.Run(&deployer) ? 0 : 1;
  }

  if (argc >= 1 && option == "--add-schema") {
    return add_schema(argc, argv);
  }

  if (argc == 1 && option == "--set-active-schema") {
    return set_active_schema(argv[0]);
  }

  if (argc >= 1 && option == "--compile") {
    rime::Deployer& deployer(rime::Service::instance().deployer());
    configure_deployer(&deployer, argc - 1, argv + 1);
    std::string schema_file(argv[0]);
    rime::SchemaUpdate update(schema_file);
    update.set_verbose(true);
    return update.Run(&deployer) ? 0 : 1;
  }

  std::cerr << "invalid arguments." << std::endl;
  return 1;
}
