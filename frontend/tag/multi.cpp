/*
 *		Multi-class classifier.
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

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <classias/classias.h>
#include <classias/quark.h>
#include <classias/classify/linear/multi.h>
#include <classias/evaluation.h>

#include "option.h"
#include "tokenize.h"
#include "defaultmap.h"
#include <util.h>

typedef defaultmap<std::string, double> model_type;
typedef std::vector<std::string> labels_type;
typedef std::vector<int> positive_labels_type;
typedef classias::classify::linear_multi_logistic<model_type> classifier_type;

class feature_generator
{
public:
    typedef std::string attribute_type;
    typedef std::string label_type;
    typedef std::string feature_type;

public:
    feature_generator()
    {
    }

    virtual ~feature_generator()
    {
    }

    inline bool forward(
        const std::string& a,
        const std::string& l,
        std::string& f
        ) const
    {
        f  = a;
        f += '\t';
        f += l;
        return true;
    }
};

static void
parse_line(
    classifier_type& inst,
    const feature_generator& fgen,
    std::string& rl,
    const classias::quark& labels,
    const option& opt,
    const std::string& line,
    int lines = 0
    )
{
    double value;
    std::string name;

    // Split the line with tab characters.
    tokenizer values(line, opt.token_separator);
    tokenizer::iterator itv = values.begin();
    if (itv == values.end()) {
        throw invalid_data("no field found in the line", line, lines);
    }

    // Parse the instance label.
    get_name_value(*itv, name, value, opt.value_separator);
    rl = name;

    // Initialize the classifier.
    inst.clear();
    inst.resize(labels.size());

    // Set attributes for the instance.
    for (++itv;itv != values.end();++itv) {
        if (!itv->empty()) {
            double value;
            std::string name;
            get_name_value(*itv, name, value, opt.value_separator);

            for (int i = 0;i < (int)labels.size();++i) {
                inst.set(i, fgen, name, labels.to_item(i), value);
            }
        }
    }

    // Apply the bias feature if any.
    for (int i = 0;i < (int)labels.size();++i) {
        inst.set(i, fgen, "__BIAS__", labels.to_item(i), 1.0);
    }

    // Finalize the instance.
    inst.finalize();
}

static void
read_model(
    model_type& model,
    classias::quark& labels,
    std::istream& is,
    option& opt
    )
{
    for (;;) {
        std::string line;
        std::getline(is, line);
        if (is.eof()) {
            break;
        }

        // Candidate label.
        if (line.compare(0, 7, "@label\t") == 0) {
            labels(line.substr(7));
            continue;
        }

        if (line.compare(0, 1, "@") == 0) {
            continue;
        }

        int pos = line.find('\t');
        if (pos == line.npos) {
            throw invalid_model("feature weight is missing", line);
        }

        double w = std::atof(line.c_str());
        if (++pos == line.size()) {
            throw invalid_model("feature name is missing", line);
        }

        model.insert(model_type::pair_type(line.substr(pos), w));
    }
}

static void output_model_label(
    std::ostream& os,
    classifier_type& inst,
    const classias::quark& labels,
    option& opt
    )
{
}

int multi_tag(option& opt, std::ifstream& ifs)
{
    int lines = 0;
    feature_generator fgen;
    std::istream& is = opt.is;
    std::ostream& os = opt.os;

    // Load a model.
    model_type model;
    classias::quark labels;
    read_model(model, labels, ifs, opt);

    // Create an instance of a classifier on the model.
    classifier_type inst(model);

    // Generate a set of positive labels (necessary only for evaluation).
    positive_labels_type positives;
    if (opt.test) {
        for (int i = 0;i < (int)labels.size();++i) {
            if (opt.negative_labels.find(labels.to_item(i)) == opt.negative_labels.end()) {
                positives.push_back(i);
            }
        }
    }

    // Objects for performance evaluation.
    classias::accuracy acc;
    classias::precall pr(labels.size());

    // Create another quark for labels unseen in the training stage.
    classias::quark rlabels = labels;

    for (;;) {
        // Read a line.
        std::string line;
        std::getline(is, line);
        if (is.eof()) {
            break;
        }
        ++lines;

        // An empty line or comment line.
        if (line.empty() || line.compare(0, 1, "#") == 0) {
            // Output the comment line if necessary.
            if (opt.output & option::OUTPUT_COMMENT) {
                os << line << std::endl;
            }
            continue;
        }

        // Parse the line and classify the instance.
        std::string rlabel;
        parse_line(inst, fgen, rlabel, labels, opt, line, lines);

        // Determine whether we output this instance or not.
        if (opt.condition == option::CONDITION_ALL ||
            (opt.condition == option::CONDITION_FALSE && labels.to_item(inst.argmax()) != rlabel)) {
            if (opt.output & option::OUTPUT_ALL) {
                // Output all candidates
                os << "@boi" << std::endl;

                for (int i = 0;i < inst.size();++i) {
                    // Output the reference label.
                    if (opt.output & option::OUTPUT_RLABEL) {
                        os << ((labels.to_item(i) == rlabel) ? '+' : '-');
                    }
                    // Output the predicted label.
                    os << ((i == inst.argmax()) ? '+' : '-');
                    os << labels.to_item(i);

                    // Output the score/probability if necessary.
                    if (opt.output & option::OUTPUT_PROBABILITY) {
                        os << opt.value_separator << inst.prob(i);
                    } else if (opt.output & option::OUTPUT_SCORE) {
                        os << opt.value_separator << inst.score(i);
                    }

                    os << std::endl;
                }

                os << "@eoi" << std::endl;

            } else  {
                // Output the predicted candidate only.

                // Output the reference label.
                if (opt.output & option::OUTPUT_RLABEL) {
                    os << rlabel << opt.token_separator;
                }

                // Output the predicted label.
                os << labels.to_item(inst.argmax());

                // Output the score/probability if necessary.
                if (opt.output & option::OUTPUT_PROBABILITY) {
                    os << opt.value_separator << inst.prob(inst.argmax());
                } else if (opt.output & option::OUTPUT_SCORE) {
                    os << opt.value_separator << inst.score(inst.argmax());
                }

                os << std::endl;
            }

        }

        // Accumulate the performance.
        if (opt.test) {
            int pl = inst.argmax();
            int rl = rlabels.to_value(rlabel, rlabels.size());
            if (rl != rlabels.size()) {
                acc.set(pl == rl);
                pr.set(pl, rl);
            } else {
                int rl = rlabels(rlabel);
                pr.resize(rlabels.size());
                pr.set(pl, rl);
            }
        }
    }

    // Output the performance if necessary.
    if (opt.test) {
        acc.output(os);
        pr.output_labelwise(os, labels, positives.begin(), positives.end());
        pr.output_micro(os, positives.begin(), positives.end());
        pr.output_macro(os, positives.begin(), positives.end());
    }

    return 0;
}
