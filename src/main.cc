/*****************************************************************************\
 Wendy -- Synthesizing Partners for Services

 Copyright (c) 2009 Niels Lohmann, Christian Sura, and Daniela Weinberg

 Wendy is free software: you can redistribute it and/or modify it under the
 terms of the GNU Affero General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.

 Wendy is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
 more details.

 You should have received a copy of the GNU Affero General Public License
 along with Wendy.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/


#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <libgen.h>
#include "config.h"
#include "config-log.h"
#include "StoredKnowledge.h"
#include "PossibleSendEvents.h"
#include "Label.h"
#include "Output.h"
#include "Cover.h"
#include "verbose.h"

// detect MinGW compilation under Cygwin
#ifndef CYGWIN_MINGW
#ifdef WIN32
#ifndef _WIN32
#define CYGWIN_MINGW 1
#endif
#endif
#endif

using std::string;


/// input files
extern FILE *graph_in;
extern FILE *cover_in;

/// the parsers
extern int graph_parse();
extern int cover_parse();

/// the command line parameters
gengetopt_args_info args_info;

/// the invocation string
string invocation;

/// a file to store a mapping from marking ids to actual Petri net markings
Output *markingoutput = NULL;


bool fileExists(std::string filename) {
    FILE *tmp = fopen(filename.c_str(), "r");
    if (tmp) {
        fclose(tmp);
        return true;
    } else {
        return false;
    }
}


/// evaluate the command line parameters
void evaluateParameters(int argc, char** argv) {
    // overwrite invocation for consistent error messages
    argv[0] = (char*)PACKAGE;

    // store invocation in a string for meta information in file output
    for (int i = 0; i < argc; ++i) {
        invocation += string(argv[i]) + " ";
    }

    // set default values
    cmdline_parser_init(&args_info);

    // initialize the parameters structure
    struct cmdline_parser_params *params = cmdline_parser_params_create();

    // call the cmdline parser
    if (cmdline_parser(argc, argv, &args_info) != 0) {
        abort(7, "invalid command-line parameter(s)");
    }

    // debug option
    if (args_info.bug_flag) {
        FILE *debug_output = fopen("bug.log", "w");
        fprintf(debug_output, "%s\n", CONFIG_LOG);
        fclose(debug_output);
        message("please send file 'bug.log' to %s!", PACKAGE_BUGREPORT);
        exit(EXIT_SUCCESS);
    }

    // read a configuration file if necessary
    if (args_info.config_given) {
        // initialize the config file parser
        params->initialize = 0;
        params->override = 0;

        // call the config file parser
        if (cmdline_parser_config_file (args_info.config_arg, &args_info, params) != 0) {
            abort(14, "error reading configuration file '%s'", args_info.config_arg);
        } else {
            status("using configuration file '%s'", args_info.config_arg);
        }
    } else {
        // check for configuration files
        string conf_filename = fileExists("wendy.conf") ? "wendy.conf" :
                               (fileExists(string(SYSCONFDIR) + "/wendy.conf") ?
                               (string(SYSCONFDIR) + "/wendy.conf") : "");

        if (conf_filename != "") {
            // initialize the config file parser
            params->initialize = 0;
            params->override = 0;
            if (cmdline_parser_config_file ((char*)conf_filename.c_str(), &args_info, params) != 0) {
                abort(14, "error reading configuration file '%s'", conf_filename.c_str());
            } else {
                status("using configuration file '%s'", conf_filename.c_str());
            }
        } else {
            status("not using a configuration file");
        }
    }

    // initialize the report frequency
    if (args_info.reportFrequency_arg < 0) {
        abort(8, "report frequency must not be negative");
    }
    StoredKnowledge::reportFrequency = args_info.reportFrequency_arg;

    // check whether at most one file is given
    if (args_info.inputs_num > 1) {
        abort(4, "at most one input file must be given");
    }

    if (args_info.sa_given and args_info.og_given) {
        abort(12, "'--og' and '--sa' parameter are mutually exclusive");
    }

    free(params);
}


int main(int argc, char** argv) {
    // set a standard filename
    string filename("wendy_output");
    time_t start_time, end_time;

    /*--------------------------------------.
    | 0. parse the command line parameters  |
    `--------------------------------------*/
    evaluateParameters(argc, argv);


    /*----------------------.
    | 1. parse the open net |
    `----------------------*/
    try {
        // parse either from standard input or from a given file
        if (args_info.inputs_num == 0) {
            std::cin >> pnapi::io::owfn >> *InnerMarking::net;
        } else {
            // strip suffix from input filename
            filename = string(args_info.inputs[0]).substr(0,string(args_info.inputs[0]).find_last_of("."));

            std::ifstream inputStream(args_info.inputs[0]);
            if (!inputStream) {
                abort(1, "could not open file '%s'", args_info.inputs[0]);
            }
            inputStream >> meta(pnapi::io::INPUTFILE, args_info.inputs[0])
                        >> pnapi::io::owfn >> *InnerMarking::net;
        }
        if (args_info.verbose_flag) {
            std::ostringstream s;
            s << pnapi::io::stat << *InnerMarking::net;
            status("read net: %s", s.str().c_str());
        }
    } catch (pnapi::io::InputError error) {
        std::stringstream s;
        s << error;
        abort(2, "\b%s", s.str().c_str());
    }

    // "fix" the net in order to avoid parse errors from LoLA (see bug #14166)
    if (InnerMarking::net->getTransitions().empty()) {
        status("net has no transitions -- adding dead dummy transition");
        InnerMarking::net->createArc(InnerMarking::net->createPlace(),
                                     InnerMarking::net->createTransition());
    }

    // only normal nets are supported so far
    if (not InnerMarking::net->isNormal()) {
        abort(3, "the input open net must be normal");
    }


    /*--------------------------------------------.
    | 2. initialize labels and interface markings |
    `--------------------------------------------*/
    Label::initialize();
    InterfaceMarking::initialize(args_info.messagebound_arg);
    PossibleSendEvents::initialize();


    /*----------------------------.
    | 3. read cover file if given |
    `----------------------------*/
    if (args_info.cover_given) {
        if (args_info.cover_arg) {
            cover_in = fopen(args_info.cover_arg, "r");
            if (cover_in == NULL) {
                abort(15, "could not open cover file '%s'", args_info.cover_arg);
            }
            cover_parse();
            fclose(cover_in);

            status("read cover file '%s'", args_info.cover_arg);
        } else {
            Cover::coverAll();
            status("covering all nodes");
        }
        status("%d nodes to cover", Cover::nodeCount);
    }


    /*--------------------------------------------.
    | 4. write inner of the open net to LoLA file |
    `--------------------------------------------*/
    Output temp;
    temp.stream() << pnapi::io::lola << *InnerMarking::net;


    /*------------------------------------------.
    | 5. call LoLA and parse reachability graph |
    `------------------------------------------*/
    time(&start_time);

    // marking information output
    if (args_info.mi_given) {
        string mi_filename = args_info.mi_arg ? args_info.mi_arg : filename + ".mi";
        markingoutput = new Output(mi_filename, "marking information");
    }

    // select LoLA binary
#if defined(CYGWIN_MINGW)
    // MinGW does not understand pathnames with "/", so we use the basename
    string lola_command = basename(args_info.lola_arg);
#else
    string lola_command = args_info.lola_arg;
#endif

    // read from a pipe or from a file (MinGW cannot pipe)
#if defined(HAVE_POPEN) && defined(HAVE_PCLOSE) && !defined(CYGWIN_MINGW)
    string command_line = lola_command + " " + temp.name() + " -M 2> /dev/null";
    status("creating a pipe to LoLA by calling '%s'", command_line.c_str());
    graph_in = popen(command_line.c_str(), "r");
    graph_parse();
    pclose(graph_in);
#else
    string command_line = lola_command + " " + temp.filename + " -m";
#if !defined(CYGWIN_MINGW)
    // MinGW cannot pipe
    command_line += " &> /dev/null";
#endif
    status("calling LoLA with '%s'", command_line.c_str());
    system(command_line.c_str());
    graph_in = fopen((temp.filename + ".graph").c_str(), "r");
    graph_parse();
    fclose(graph_in);
#endif
    time(&end_time);

    // close marking information output file
    if (args_info.mi_given) {
        delete markingoutput;
    }


    /*-------------------------------.
    | 6. organize reachability graph |
    `-------------------------------*/
    InnerMarking::initialize();
    if (args_info.cover_given) {
        Cover::clear();
    }

    status("LoLA is done [%.0f sec]", difftime(end_time, start_time));


    /*-------------------------------.
    | 7. calculate knowledge bubbles |
    `-------------------------------*/
    time(&start_time);
    {
        Knowledge K0(0);
        StoredKnowledge::root = new StoredKnowledge(K0);

        if (StoredKnowledge::root->is_sane) {
            StoredKnowledge::root->store();
            StoredKnowledge::processRecursively(K0, StoredKnowledge::root);
        }
    }
    time(&end_time);

    // statistics output
    status("stored %d knowledges [%.0f sec]",
        StoredKnowledge::stats.storedKnowledges, difftime(end_time, start_time));
    status("used %d of %d hash buckets, maximal bucket size: %d",
        static_cast<unsigned int>(StoredKnowledge::hashTree.size()),
        (1 << (8*sizeof(hash_t))), static_cast<unsigned int>(StoredKnowledge::stats.maxBucketSize));
    status("at most %d interface markings per inner marking",
        StoredKnowledge::stats.maxInterfaceMarkings);
    status("calculated %d trivial SCCs",
        StoredKnowledge::stats.numberOfTrivialSCCs);
    status("calculated %d non-trivial SCCs, at most %d members in nontrivial SCC",
        StoredKnowledge::stats.numberOfNonTrivialSCCs, StoredKnowledge::stats.maxSCCSize);

    // traverse all nodes reachable from the root
    StoredKnowledge::root->traverse();
    status("%d sane nodes reachable", StoredKnowledge::seen.size());


    // analyze root node and print result
    message("net is controllable: %s", (StoredKnowledge::root->is_sane) ? "YES" : "NO");


    /*-------------------------------.
    | 8. calculate cover constraint |
    `--------------------------------*/
    if (args_info.cover_given) {
        Cover::calculate(StoredKnowledge::seen);

        message("cover constraint is satisfiable: %s", (Cover::satisfiable) ? "YES" : "NO");
    }


    /*-------------------.
    | 9. output options |
    `-------------------*/
    // operating guidelines output
    if (args_info.og_given) {
        string og_filename = args_info.og_arg ? args_info.og_arg : filename + ".og";
        Output output(og_filename, "operating guidelines");

        if (args_info.fionaFormat_flag) {
            StoredKnowledge::output_ogold(output);
        } else {
            StoredKnowledge::output_og(output);
        }

        if (args_info.cover_given) {
            string cover_filename = og_filename + ".cover";
            Output cover_output(cover_filename, "cover constraint");
            Cover::write(cover_output);
        }
    }

    // service automaton output
    if (args_info.sa_given) {
        string sa_filename = args_info.sa_arg ? args_info.sa_arg : filename + ".sa";
        Output output(sa_filename, "service automaton");
        StoredKnowledge::output_og(output);
    }

    // dot output
    if (args_info.dot_given) {
        string dot_filename = args_info.dot_arg ? args_info.dot_arg : filename + ".dot";
        Output output(dot_filename, "dot representation");
        StoredKnowledge::output_dot(output);
    }

    // migration output
    if (args_info.im_given) {
        string im_filename = args_info.im_arg ? args_info.im_arg : filename + ".im";
        Output output(im_filename, "migration information");

        time(&start_time);
        StoredKnowledge::output_migration(output);
        time(&end_time);

        status("wrote migration information to file '%s' [%.0f sec]",
            im_filename.c_str(), difftime(end_time, start_time));
    }


    /*-------------------.
    | 10. release memory |
    `-------------------*/
    if (args_info.finalize_flag) {
        time(&start_time);
        cmdline_parser_free(&args_info);
        InnerMarking::finalize();
        StoredKnowledge::finalize();
        time(&end_time);
        status("released memory [%.0f sec]", difftime(end_time, start_time));
    }

    return EXIT_SUCCESS;
}
