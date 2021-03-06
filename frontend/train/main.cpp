/*
 *		Classias frontend for training.
 *
 * Copyright (c) 2008,2009 Naoaki Okazaki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of the authors nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id$ */

#ifdef  HAVE_CONFIG_H
#include <config.h>
#endif/*HAVE_CONFIG_H*/

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <typeinfo>
#include <classias/version.h>
#include <optparse.h>
#include <tokenize.h>

#include "option.h"

int binary_train(option& opt);
int multi_train(option& opt);
int candidate_train(option& opt);

class optionparser : public option, public optparse
{
protected:
    typedef std::map<std::string, std::string> synonyms_type;
    synonyms_type m_algorithms;

public:
    optionparser(
        std::istream* _is = &std::cin,
        std::ostream* _os = &std::cout,
        std::ostream* _es = &std::cerr
        ) : option(_is, _os, _es)
    {
        // Build synsets for algorithms.
        m_algorithms["lbfgs.logistic"]              = "lbfgs.logistic";
        m_algorithms["lbfgs"]                       = "lbfgs.logistic";
        m_algorithms["averaged_perceptron"]         = "averaged_perceptron";
        m_algorithms["ap"]                          = "averaged_perceptron";
        m_algorithms["pegasos.logistic"]            = "pegasos.logistic";
        m_algorithms["pegasos.hinge"]               = "pegasos.hinge";
        m_algorithms["pegasos.svm"]                 = "pegasos.hinge";
        m_algorithms["truncated_gradient.logistic"] = "truncated_gradient.logistic";
        m_algorithms["tg.logistic"]                 = "truncated_gradient.logistic";
        m_algorithms["truncated_gradient.hinge"]    = "truncated_gradient.hinge";
        m_algorithms["tg.hinge"]                    = "truncated_gradient.hinge";
        m_algorithms["tg.svm"]                      = "truncated_gradient.hinge";
    }

    BEGIN_OPTION_MAP_INLINE()
        ON_OPTION_WITH_ARG(SHORTOPT('t') || LONGOPT("type"))
            if (strcmp(arg, "binary") == 0 || strcmp(arg, "b") == 0) {
                type = TYPE_BINARY;
            } else if (strcmp(arg, "multi-dense") == 0 || strcmp(arg, "m") == 0) {
                type = TYPE_MULTI_DENSE;
            } else if (strcmp(arg, "multi-sparse") == 0 || strcmp(arg, "n") == 0) {
                type = TYPE_MULTI_SPARSE;
            } else if (strcmp(arg, "candidate") == 0 || strcmp(arg, "c") == 0) {
                type = TYPE_CANDIDATE;
            } else {
                std::stringstream ss;
                ss << "unknown data format specified: " << arg;
                throw invalid_value(ss.str());
            }

        ON_OPTION_WITH_ARG(SHORTOPT('a') || LONGOPT("algorithm"))
            synonyms_type::const_iterator it = m_algorithms.find(arg);
            if (it != m_algorithms.end()) {
                algorithm = it->second;
            } else {
                std::stringstream ss;
                ss << "unknown training algorithm specified: " << arg;
                throw invalid_value(ss.str());
            }

        ON_OPTION_WITH_ARG(SHORTOPT('p') || LONGOPT("set"))
            params.push_back(arg);

        ON_OPTION(SHORTOPT('f') || LONGOPT("shuffle"))
            shuffle = true;

        ON_OPTION_WITH_ARG(SHORTOPT('b') || LONGOPT("bias"))
            bias = atof(arg);

        ON_OPTION_WITH_ARG(SHORTOPT('m') || LONGOPT("model"))
            model = arg;

        ON_OPTION_WITH_ARG(SHORTOPT('g') || LONGOPT("split"))
            split = atoi(arg);

        ON_OPTION_WITH_ARG(SHORTOPT('e') || LONGOPT("holdout"))
            holdout = atoi(arg);

        ON_OPTION(SHORTOPT('x') || LONGOPT("cross-validate"))
            cross_validation = true;

#if defined(HAVE_REGEX) || defined(HAVE_BOOST_REGEX_HPP)
        ON_OPTION_WITH_ARG(SHORTOPT('F') || LONGOPT("filter"))
            filter = arg;
            filter_string = arg;
#endif/*defined(HAVE_REGEX) || defined(HAVE_BOOST_REGEX_HPP)*/

        ON_OPTION_WITH_ARG(SHORTOPT('n') || LONGOPT("negative"))
            negative_labels.insert(arg);

        ON_OPTION(SHORTOPT('l') || LONGOPT("log-to-file"))
            logfile = true;

        ON_OPTION_WITH_ARG(SHORTOPT('L') || LONGOPT("logbase"))
            logbase = arg;

        ON_OPTION_WITH_ARG(SHORTOPT('s') || LONGOPT("token-separator"))
            if (strcmp(arg, " ") == 0 || strcasecmp(arg, "s") == 0 || strcasecmp(arg, "spc") == 0 || strcasecmp(arg, "space") == 0) {
                token_separator = ' ';
            } else if (strcmp(arg, ",") == 0 || strcasecmp(arg, "c") == 0 || strcasecmp(arg, "comma") == 0) {
                token_separator = ',';
            } else if (strcmp(arg, "\t") == 0 || strcasecmp(arg, "t") == 0 || strcasecmp(arg, "tab") == 0) {
                token_separator = '\t';
            } else {
                std::stringstream ss;
                ss << "unknown token separator specified: " << arg;
                throw invalid_value(ss.str());
            }

        ON_OPTION_WITH_ARG(SHORTOPT('c') || LONGOPT("value-separator"))
            if (strcmp(arg, ":") == 0 || strcasecmp(arg, "c") == 0 || strcasecmp(arg, "colon") == 0) {
                value_separator = ':';
            } else if (strcmp(arg, "=") == 0 || strcasecmp(arg, "e") == 0 || strcasecmp(arg, "eq") == 0 || strcasecmp(arg, "equal") == 0) {
                value_separator = '=';
            } else if (strcmp(arg, "|") == 0 || strcasecmp(arg, "b") == 0 || strcasecmp(arg, "bar") == 0) {
                value_separator = '|';
            } else {
                std::stringstream ss;
                ss << "unknown value separator specified: " << arg;
                throw invalid_value(ss.str());
            }

        ON_OPTION(SHORTOPT('h') || LONGOPT("help"))
            mode = MODE_HELP;

        ON_OPTION(SHORTOPT('H') || LONGOPT("help-parameters"))
            mode = MODE_HELP_ALGORITHM;

    END_OPTION_MAP()
};

static void usage(std::ostream& os, const char *argv0)
{
    os << "USAGE: " << argv0 << " [OPTIONS] [DATA1] [DATA2] ..." << std::endl;
    os << "This utility trains a model from training data set(s)." << std::endl;
    os << std::endl;
    os << "  DATA    file(s) corresponding to data set(s) for training; if multiple N files" << std::endl;
    os << "          are specified, this utility assigns a group number (1...N) to the" << std::endl;
    os << "          instances in each file if no file is specified, the utility reads a" << std::endl;
    os << "          data set from STDIN; if a file name has an extension '.gz', '.bz2'," << std::endl;
    os << "          and '.xz', the utility uses 'gzip', 'bzip2', and 'xv' to decompress" << std::endl;
    os << "          the input file" << std::endl;
    os << std::endl;
    os << "OPTIONS:" << std::endl;
    os << "  -t, --type=TYPE       specify a task type (DEFAULT='multi-dense'):" << std::endl;
    os << "      b, binary             an instance consists of a boolean class, +1 or -1," << std::endl;
    os << "                            and features" << std::endl;
    os << "      m, multi-dense        an instance consists of a label and attributes;" << std::endl;
    os << "                            features are automatically generated by combining" << std::endl;
    os << "                            possible pairs of attributes and labels regardless" << std::endl;
    os << "                            of the appearances in the training set" << std::endl;
    os << "      n, multi-sparse       an instance consists of a label and attributes;" << std::endl;
    os << "                            features are automatically generated by pairing" << std::endl;
    os << "                            attributes and labels appearing in the training set" << std::endl;
    os << "      c, candidate          an instance begins with a directive line '@boi'" << std::endl;
    os << "                            followed by lines that correspond to multiple" << std::endl;
    os << "                            candidates for the instance; a candidate line" << std::endl;
    os << "                            consists of a class label and features; an instance" << std::endl;
    os << "                            ends with a directive line '@eoi'" << std::endl;
    os << "  -a, --algorithm=NAME  specify a training algorithm (DEFAULT='lbfgs.logistic')" << std::endl;
    os << "      lbfgs.logistic        L1/L2-regularized logistic regression (LR) by L-BFGS" << std::endl;
    os << "      averaged_perceptron   averaged perceptron" << std::endl;
    os << "      pegasos.logistic      L2-regularized LR by Pegasos" << std::endl;
    os << "      pegasos.hinge         L2-regularized linear L1-loss SVM by Pegasos" << std::endl;
    os << "      truncated_gradient.logistic" << std::endl;
    os << "                            L1-regularized LR by Truncated Gradient" << std::endl;
    os << "      truncated_gradient.hinge" << std::endl;
    os << "                            L1-regularized L1-loss SVM by Truncated Gradient" << std::endl;
    os << "  -p, --set=NAME=VALUE  set the algorithm-specific parameter NAME to VALUE;" << std::endl;
    os << "                        use '-H' or '--help-parameters' with the algorithm name" << std::endl;
    os << "                        specified by '-a' or '--algorithm' and the task type" << std::endl;
    os << "                        specified by '-t' or '--type' to see the list of the" << std::endl;
    os << "                        algorithm-specific parameters" << std::endl;
    os << "  -f, --shuffle         shuffle (reorder) instances in the data" << std::endl;
    os << "  -b, --bias=VALUE      insert bias features with their values VALUE" << std::endl;
    os << "  -m, --model=FILE      store the model to FILE (DEFAULT=''); if the value is" << std::endl;
    os << "                        empty, this utility does not store the model" << std::endl;
    os << "  -g, --split=N         split the instances into N groups; this option is" << std::endl;
    os << "                        useful for holdout evaluation and cross validation" << std::endl;
    os << "  -e, --holdout=M       use the M-th data for holdout evaluation and the rest" << std::endl;
    os << "                        for training" << std::endl;
    os << "  -x, --cross-validate  repeat holdout evaluations for #i in {1, ..., N}" << std::endl;
    os << "                        (N-fold cross validation)" << std::endl;
    os << "  -l, --log-to-file     write the training log to a file instead of to STDOUT;" << std::endl;
    os << "                        The filename is determined automatically by the training" << std::endl;
    os << "                        algorithm, parameters, and source files" << std::endl;
    os << "  -L, --logbase=BASE    set the base name for a log file (used with -l option)" << std::endl;
#if     defined(HAVE_REGEX) || defined(HAVE_BOOST_REGEX_HPP)
    os << "  -F, --filter=REGEX    filter attributes whose names are matched by REGEX" << std::endl;
#endif/*defined(HAVE_REGEX) || defined(HAVE_BOOST_REGEX_HPP)*/
    os << "  -n, --negative=LABEL  specify a negative label for computing precision," << std::endl;
    os << "                        recall, and F1 scores" << std::endl;
    os << "  -s, --token-separator=SEP assume SEP character as a token separator:" << std::endl;
    os << "      ' ',  s, spc, space       a SPACE (' ') character (DEFAULT)" << std::endl;
    os << "      '\\t', t, tab              a TAB ('\\t') character" << std::endl;
    os << "      ',',  c, comma            a COMMA (',') character" << std::endl;
    os << "  -c, --value-separator=SEP assume SEP character as a value separator:" << std::endl;
    os << "      ':',  c, colon            a COLON (':') character (DEFAULT)" << std::endl;
    os << "      '=',  e, equal            a EQUAL ('=') character" << std::endl;
    os << "      '|',  b, bar              a BAR ('|') character" << std::endl;
    os << "  -h, --help            show this help message and exit" << std::endl;
    os << "  -H, --help-parameters show the help message of algorithm-specific parameters;" << std::endl;
    os << "                        specify an algorithm with '-a' or '--algorithm' option," << std::endl;
    os << "                        and specify a task type with '-t' or '--task' option" << std::endl;
    os << std::endl;
}

void output_copyright(std::ostream& os)
{
    os << CLASSIAS_NAME " ";
    os << CLASSIAS_VERSION << " ";
    os << "trainer ";
    os << CLASSIAS_COPYRIGHT << std::endl;
    os << std::endl;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    int arg_used = 0;
    optionparser opt;
    std::istream& is = *opt.is;
    std::ostream& os = *opt.os;
    std::ostream& es = *opt.es;
    std::ofstream ofs;

    // Show the copyright information.
    output_copyright(os);

    // Parse the command-line options.
    try { 
        arg_used = opt.parse(argv, argc);
    } catch (const optparse::unrecognized_option& e) {
        es << "ERROR: unrecognized option: " << e.what() << std::endl;
        return 1;
    } catch (const optparse::invalid_value& e) {
        es << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    // Show the help message and exit.
    if (opt.mode == option::MODE_HELP) {
        usage(os, argv[0]);
        return ret;
    }

    // Set the source files.
    for (int i = arg_used;i < argc;++i) {
        opt.files.push_back(argv[i]);
    }

    // Open a log file if necessary.
    if (opt.logfile) {
        // Generate a filename for the log file.
        std::string fname;

        if (opt.logbase.empty()) {
            fname += "log.classias-train";
        } else {
            fname += opt.logbase;
        }

        fname += '_';
        fname += opt.algorithm;
        for (int i = 0;i < (int)opt.params.size();++i) {
            fname += '_';
            fname += opt.params[i];
        }

        ofs.open(fname.c_str());
        if (ofs.fail()) {
            es << "ERROR: failed to open the log file: " << fname << std::endl;
            return 1;
        }

        output_copyright(ofs);
        opt.os = &ofs;
    }

    // Branch for tasks.
    try {
        switch (opt.type) {
        case option::TYPE_BINARY:
            ret = binary_train(opt);
            break;
        case option::TYPE_MULTI_SPARSE:
        case option::TYPE_MULTI_DENSE:
            ret = multi_train(opt);
            break;
        case option::TYPE_CANDIDATE:
            ret = candidate_train(opt);
            break;
        }
    } catch (const std::exception& e) {
        es << "ERROR: " << typeid(e).name() << ": " << e.what() << std::endl;
        return 1;
    }

    return ret;
}
