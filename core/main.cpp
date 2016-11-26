
#include "pdx.hpp"
#include "error.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <string>
#include <fstream>
#include <iostream>

using namespace std;
using namespace boost::filesystem;
namespace po = boost::program_options;


int main(int argc, const char** argv) {
    try {
        path opt_game_path;

        /* command-line & configuration file parameter specification */

        po::options_description opt_spec{ "Options" };
        opt_spec.add_options()
            ("help,h", "Show help information")
            ("config,c",
                po::value<path>(),
                "Configuration file")
            ("game-path",
                po::value<path>(&opt_game_path)->default_value("C:/Program Files (x86)/Steam/steamapps/common/Crusader Kings II"),
                "Path to game folder")
            ("mod-path",
                po::value<path>(),
                "Path to root folder of a mod")
            ("submod-path",
                po::value<path>(),
                "Path to root folder of a sub-mod")
            ;

        /* parse command line & optional configuration file (command-line options override --config file options)
         *
         * example config file contents:
         *
         *   game-path = C:/SteamLibrary/steamapps/common/Crusader Kings II
         *   mod-path  = D:\git\SWMH-BETA\SWMH
         */

        po::variables_map opt;
        po::store(po::parse_command_line(argc, argv, opt_spec), opt);

        if (opt.count("config")) {
            const string cfg_path = opt["config"].as<path>().string();
            std::ifstream f_cfg{ cfg_path };

            if (f_cfg)
                po::store(po::parse_config_file(f_cfg, opt_spec), opt);
            else
                throw runtime_error("failed to open config file specified with --config: " + cfg_path);
        }

        if (opt.count("help")) {
            cout << opt_spec << endl;
            return 0;
        }

        po::notify(opt);

        pdx::vfs vfs{ opt_game_path };

        if (opt.count("mod-path")) {
            vfs.push_mod_path(opt["mod-path"].as<path>());

            if (opt.count("submod-path"))
                vfs.push_mod_path(opt["submod-path"].as<path>());
        }
        else if (opt.count("submod-path"))
            throw runtime_error("cannot specify --submod-path without also providing a --mod-path");

        /* done with program option processing */

        pdx::parser("file.txt");
    }
    catch (const exception& e) {
        cerr << "fatal: " << e.what() << endl;
        return 1;
    }

    return 0;
}
