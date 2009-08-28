/*
 *		Gradient descent using L-BFGS.
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

#ifndef __CLASSIAS_TRAIN_LBFGS_H__
#define __CLASSIAS_TRAIN_LBFGS_H__

#include <cmath>
#include <ctime>
#include <float.h>
#include <iostream>
#include <limits.h>
#include <set>
#include <string>
#include <vector>

#include <lbfgs.h>

#include <classias/types.h>
#include <classias/parameters.h>
#include <classias/evaluation.h>
#include <classias/classify/linear/binary.h>
#include <classias/classify/linear/multi.h>

namespace classias
{

namespace train
{


/**
 * The base class for gradient descent using L-BFGS.
 *  This class implements internal variables, operations, and interface
 *  that are common for training a binary/multi classification.
 *
 *  @param  model_tmpl  The type of a weight vector for features.
 */
template <
    class model_tmpl
>
class lbfgs_base
{
public:
    /// The type implementing a model (weight vector for features).
    typedef model_tmpl model_type;
    /// The type representing a value.
    typedef typename model_type::value_type value_type;
    /// A synonym of this class.
    typedef lbfgs_base<model_tmpl> this_class;

protected:
    /// The array of feature weights.
    model_type m_w;

    /// Parameter interface.
    parameter_exchange m_params;

    /// Sigma for L1-regularization;
    value_type m_regularization_sigma1;
    /// Sigma for L2-regularization;
    value_type m_regularization_sigma2;
    /// The number of memories in L-BFGS.
    int m_lbfgs_num_memories;
    /// L-BFGS epsilon for convergence.
    value_type m_lbfgs_epsilon;
    /// Number of iterations for stopping criterion.
    int m_lbfgs_stop;
    /// The delta threshold for stopping criterion.
    value_type m_lbfgs_delta;
    /// Maximum number of L-BFGS iterations.
    int m_lbfgs_maxiter;
    /// Line search algorithm.
    std::string m_lbfgs_linesearch;
    /// The maximum number of trials for the line search algorithm.
    int m_lbfgs_max_linesearch;

    /// A group number for holdout evaluation.
    int m_holdout;
    /// An output stream to which this object outputs log messages.
    std::ostream* m_os;

    /// An internal variable (previous timestamp).
    clock_t m_clk_prev;
    /// The start index for regularization.
    int m_regularization_start;

public:
    /**
     * Constructs the object.
     */
    lbfgs_base()
    {
        clear();
    }

    /**
     * Destructs the object.
     */
    virtual ~lbfgs_base()
    {
    }

    /**
     * Resets the internal states and parameters to default.
     */
    void clear()
    {
        m_w.clear();

        // Initialize the members.
        m_holdout = -1;
        m_os = NULL;

        // Initialize the parameters.
        m_params.init("regularization.sigma1", &m_regularization_sigma1, 0.0,
            "Coefficient (sigma) for L1-regularization.");
        m_params.init("regularization.sigma2", &m_regularization_sigma2, 1.0,
            "Coefficient (sigma) for L2-regularization.");
        m_params.init("lbfgs.num_memories", &m_lbfgs_num_memories, 6,
            "The number of corrections to approximate the inverse hessian matrix.");
        m_params.init("lbfgs.epsilon", &m_lbfgs_epsilon, 1e-5,
            "Epsilon for testing the convergence of the log likelihood.");
        m_params.init("lbfgs.stop", &m_lbfgs_stop, 10,
            "The duration of iterations to test the stopping criterion.");
        m_params.init("lbfgs.delta", &m_lbfgs_delta, 1e-5,
            "The threshold for the stopping criterion; an L-BFGS iteration stops when the\n"
            "improvement of the log likelihood over the last ${lbfgs.stop} iterations is\n"
            "no greater than this threshold.");
        m_params.init("lbfgs.max_iterations", &m_lbfgs_maxiter, INT_MAX,
            "The maximum number of L-BFGS iterations.");
        m_params.init("lbfgs.linesearch", &m_lbfgs_linesearch, "MoreThuente",
            "The line search algorithm used in L-BFGS updates:\n"
            "{'MoreThuente': More and Thuente's method, 'Backtracking': backtracking}");
        m_params.init("lbfgs.max_linesearch", &m_lbfgs_max_linesearch, 20,
            "The maximum number of trials for the line search algorithm.");
    }

    /**
     * Initializes the weight vector of the size K.
     *  This function prepares a vector of the size K, and sets W = 0.
     *  @param  K           The size of the weight vector.
     */
    void initialize_weights(const size_t K)
    {
        m_w.resize(K);
        for (size_t k = 0;k < K;++k) {
            m_w[k] = 0;
        }
    }

    static value_type
    __lbfgs_evaluate(
        void *inst, const value_type *x, value_type *g, const int n, const value_type step)
    {
        lbfgs_base* pt = reinterpret_cast<lbfgs_base*>(inst);
        return pt->lbfgs_evaluate(x, g, n, step);
    }

    value_type lbfgs_evaluate(
        const value_type *x,
        value_type *g,
        const int n,
        const value_type step
        )
    {
        // Compute the loss and gradients.
        value_type loss = loss_and_gradient(x, g, n);

	    // L2 regularization.
	    if (m_regularization_sigma2 != 0.) {
            value_type norm = 0.;
            for (int i = m_regularization_start;i < n;++i) {
                g[i] += (m_regularization_sigma2 * x[i]);
                norm += x[i] * x[i];
            }
            loss += (m_regularization_sigma2 * norm * 0.5);
	    }

        return loss;
    }

    static int
    __lbfgs_progress(
        void *inst,
        const double *x, const double *g, const double fx,
        const double xnorm, const double gnorm,
        const double step,
        int n, int k, int ls
        )
    {
        lbfgs_base* pt = reinterpret_cast<lbfgs_base*>(inst);
        int ret = pt->lbfgs_progress(x, g, fx, xnorm, gnorm, step, n, k, ls);
        if (ret != 0) {
            return LBFGSERR_MAXIMUMITERATION;
        }
        return 0;
    }

    int lbfgs_progress(
        const value_type *x,
        const value_type *g,
        const value_type fx,
        const value_type xnorm,
        const value_type gnorm,
        const value_type step,
        int n,
        int k,
        int ls)
    {
        // Compute the duration required for this iteration.
        std::ostream& os = *m_os;
        clock_t duration, clk = std::clock();
        duration = clk - m_clk_prev;
        m_clk_prev = clk;

        // Count the number of active features.
        int num_active = 0;
        for (int i = 0;i < n;++i) {
            if (x[i] != 0.) {
                ++num_active;
            }
        }

        // Output the current progress.
        os << "***** Iteration #" << k << " *****" << std::endl;
        os << "Log-likelihood: " << -fx << std::endl;
        os << "Feature norm: " << xnorm << std::endl;
        os << "Error norm: " << gnorm << std::endl;
        os << "Active features: " << num_active << " / " << n << std::endl;
        os << "Line search trials: " << ls << std::endl;
        os << "Line search step: " << step << std::endl;
        os << "Seconds required for this iteration: " <<
            duration / (double)CLOCKS_PER_SEC << std::endl;
        os.flush();

        // Holdout evaluation if necessary.
        if (m_holdout != -1) {
            holdout_evaluation();
        }

        // Output an empty line.
        os << std::endl;
        os.flush();

        return 0;
    }

    int lbfgs_solve(
        const int K,
        std::ostream& os,
        int holdout,
        int regularization_start
        )
    {
        // Set L-BFGS parameters.
        lbfgs_parameter_t param;
        lbfgs_parameter_init(&param);
        param.m = m_lbfgs_num_memories;
        param.epsilon = m_lbfgs_epsilon;
        param.past = m_lbfgs_stop;
        param.delta = m_lbfgs_delta;
        param.max_iterations = m_lbfgs_maxiter;
        if (m_lbfgs_linesearch == "Backtracking") {
            param.linesearch = LBFGS_LINESEARCH_BACKTRACKING;
        }
        param.max_linesearch = m_lbfgs_max_linesearch;
        param.orthantwise_c = m_regularization_sigma1;
        param.orthantwise_start = regularization_start;
        param.orthantwise_end = K;

        // Store the start clock.
        m_os = &os;
        m_clk_prev = clock();
        m_holdout = holdout;
        m_regularization_start = regularization_start;

        // Call L-BFGS routine.
        return lbfgs(
            K,
            &this->m_w[0],
            NULL,
            __lbfgs_evaluate,
            __lbfgs_progress,
            this,
            &param
            );
    }

    void lbfgs_output_status(std::ostream& os, int status)
    {
        if (status == LBFGS_CONVERGENCE) {
            os << "L-BFGS resulted in convergence" << std::endl;
        } else if (status == LBFGS_STOP) {
            os << "L-BFGS terminated with the stopping criteria" << std::endl;
        } else {
            os << "L-BFGS terminated with error code (" << status << ")" << std::endl;
        }
    }


    virtual value_type loss_and_gradient(
        const value_type *x,
        value_type *g,
        const int n
        ) = 0;

    virtual void holdout_evaluation() = 0;

public:
    parameter_exchange& params()
    {
        return m_params;
    }

    const model_type& model() const
    {
        return m_w;
    }
};


/**
 * Training a logistic regression model.
 */
template <
    class data_tmpl,
    class model_tmpl = weight_vector
>
class logistic_regression_binary_lbfgs : public lbfgs_base<model_tmpl>
{
public:
    /// A type representing a data set for training.
    typedef data_tmpl data_type;
    /// The type implementing a model (weight vector for features).
    typedef model_tmpl model_type;
    /// The type representing a value.
    typedef typename model_type::value_type value_type;
    /// A synonym of the base class.
    typedef lbfgs_base<model_tmpl> base_class;
    /// A synonym of this class.
    typedef logistic_regression_binary_lbfgs<data_tmpl, model_tmpl> this_class;
    /// A type representing an instance in the training data.
    typedef typename data_type::instance_type instance_type;
    /// A type providing a read-only random-access iterator for instances.
    typedef typename data_type::const_iterator const_iterator;

    /// A type representing a vector of features.
    typedef typename instance_type::features_type features_type;
    /// A type representing a feature identifier.
    typedef typename features_type::identifier_type feature_identifier_type;
    /// A classifier type.
    typedef classify::linear_binary_logistic<feature_identifier_type, value_type, model_type> classifier_type;

protected:
    /// A data set for training.
    const data_type* m_data;

public:
    logistic_regression_binary_lbfgs()
    {
    }

    virtual ~logistic_regression_binary_lbfgs()
    {
    }

    void clear()
    {
        m_data = NULL;
        base_class::clear();
    }

    virtual value_type loss_and_gradient(
        const value_type *x,
        value_type *g,
        const int n
        )
    {
        typename data_type::const_iterator iti;
        typename instance_type::const_iterator it;
        value_type loss = 0;
        classifier_type cls(this->m_w); // we know that &m_w[0] and x are identical.

        // Initialize the gradients with zero.
        for (int i = 0;i < n;++i) {
            g[i] = 0.;
        }

        // For each instance in the data.
        for (iti = m_data->begin();iti != m_data->end();++iti) {
            // Exclude instances for holdout evaluation.
            if (iti->get_group() == this->m_holdout) {
                continue;
            }

            // Compute the score for the instance.
            cls.inner_product(iti->begin(), iti->end());

            // Compute the error.
            value_type nlogp = 0.;
            value_type err = cls.error(iti->get_label(), nlogp);

            // Update the loss.
            loss += (iti->get_weight() * nlogp);

            // Update the gradients for the weights.
            err *= iti->get_weight();
            for (it = iti->begin();it != iti->end();++it) {
                g[it->first] += err * it->second;
            }
        }

        return loss;
    }

    int train(
        const data_type& data,
        std::ostream& os,
        int holdout = -1
        )
    {
        const size_t K = data.num_features();
        this->initialize_weights(K);
        
        os << "Binary logistic regression using L-BFGS" << std::endl;
        this->m_params.show(os);
        os << "lbfgs.regularization_start: " << data.get_user_feature_start() << std::endl;
        os << std::endl;

        // Call the L-BFGS solver.
        m_data = &data;
        int ret = lbfgs_solve(
            (const int)K,
            os,
            holdout,
            data.get_user_feature_start()
            );

        // Report the result from the L-BFGS solver.
        this->lbfgs_output_status(os, ret);
        return ret;
    }

    void holdout_evaluation()
    {
        classifier_type cla(this->m_w);

        holdout_evaluation_binary(
            *this->m_os,
            this->m_data->begin(),
            this->m_data->end(),
            cla,
            this->m_holdout
            );
    }
};



/**
 * Training a log-linear model using the maximum entropy modeling.
 *  @param  data_tmpl           Training data class.
 *  @param  value_tmpl          The type for computation.
 */
template <
    class data_tmpl,
    class model_tmpl = weight_vector
>
class logistic_regression_multi_lbfgs : public lbfgs_base<model_tmpl>
{
protected:
    /// A type representing a data set for training.
    typedef data_tmpl data_type;
    /// The type implementing a model (weight vector for features).
    typedef model_tmpl model_type;
    /// The type representing a value.
    typedef typename model_type::value_type value_type;
    /// A synonym of the base class.
    typedef lbfgs_base<model_tmpl> base_class;
    /// A synonym of this class.
    typedef logistic_regression_multi_lbfgs<data_type, model_tmpl> this_class;
    /// A type representing an instance in the training data.
    typedef typename data_type::instance_type instance_type;
    /// A type providing a read-only random-access iterator for instances.
    typedef typename data_type::const_iterator const_iterator;
    typedef typename instance_type::attributes_type attributes_type;
    /// A type representing a feature generator.
    typedef typename data_type::feature_generator_type feature_generator_type;
    /// A type representing a candidate for an instance.
    typedef typename data_type::attribute_type attribute_type;
    /// The type of a classifier.
    typedef classify::linear_multi_logistic<
        attribute_type, value_type, model_type> classifier_type;


    /// An array [K] of observation expectations.
    value_type *m_oexps;
    /// A data set for training.
    const data_type* m_data;

public:
    logistic_regression_multi_lbfgs()
    {
        m_oexps = NULL;
        m_data = NULL;
        clear();
    }

    virtual ~logistic_regression_multi_lbfgs()
    {
        clear();
    }

    void clear()
    {
        delete[] m_oexps;
        m_oexps = NULL;

        m_data = NULL;
        base_class::clear();
    }

    virtual value_type loss_and_gradient(
        const value_type *x,
        value_type *g,
        const int n
        )
    {
        value_type loss = 0;
        const data_type& data = *m_data;
        const int L = data.num_labels();
        classifier_type cls(this->m_w); // We know that &m_w[0] and x are identical.

        // Initialize the gradients with (the negative of) observation expexcations.
        for (int i = 0;i < n;++i) {
            g[i] = -m_oexps[i];
        }

        // For each instance in the data.
        for (const_iterator iti = data.begin();iti != data.end();++iti) {
            const instance_type& inst = *iti;

            // Exclude instances for holdout evaluation.
            if (inst.get_group() == this->m_holdout) {
                continue;
            }

            // Tell the classifier the number of possible labels.
            cls.resize(inst.num_labels(L));

            // Compute the probability prob[l] for each label #l.
            for (int l = 0;l < inst.num_labels(L);++l) {
                const attributes_type& v = inst.attributes(l);
                cls.inner_product(l, data.feature_generator, v.begin(), v.end());
            }
            cls.finalize();

            // Accumulate the model expectations of features.
            for (int l = 0;l < inst.num_labels(L);++l) {
                const attributes_type& v = inst.attributes(l);
                data.feature_generator.add_to(
                    g, v.begin(), v.end(), l, cls.prob(l));
            }

            // Accumulate the loss for predicting the instance.
            loss -= cls.logprob(inst.get_label());
        }

        return loss;
    }

    int train(
        const data_type& data,
        std::ostream& os,
        int holdout = -1
        )
    {
        const size_t K = data.num_features();
        const size_t L = data.num_labels();

        // Initialize feature expectations and weights.
        this->initialize_weights(K);
        m_oexps = new double[K];
        for (size_t k = 0;k < K;++k) {
            m_oexps[k] = 0.;
        }

        // Report the training parameters.
        os << "Multi-class logistic regression using L-BFGS" << std::endl;
        this->m_params.show(os);
        os << "lbfgs.regularization_start: " << data.get_user_feature_start() << std::endl;
        os << std::endl;

        // Compute observation expectations of the features.
        for (const_iterator iti = data.begin();iti != data.end();++iti) {
            // Skip instances for holdout evaluation.
            if (iti->get_group() == holdout) {
                continue;
            }

            // Compute the observation expectations.
            const int l = iti->get_label();
            const attributes_type& v = iti->attributes(l);
            data.feature_generator.add_to(
                m_oexps, v.begin(), v.end(), l, 1.0);
        }

        // Call the L-BFGS solver.
        m_data = &data;
        int ret = lbfgs_solve(
            (const int)K,
            os,
            holdout,
            data.get_user_feature_start()
            );

        // Report the result from the L-BFGS solver.
        this->lbfgs_output_status(os, ret);
        return ret;
    }

    void holdout_evaluation()
    {
        classifier_type cla(this->m_w);

        holdout_evaluation_multi(
            *this->m_os,
            this->m_data->begin(),
            this->m_data->end(),
            cla,
            this->m_data->feature_generator,
            this->m_holdout,
            this->m_data->positive_labels.begin(),
            this->m_data->positive_labels.end()
            );
    }
};

};

};

#endif/*__CLASSIAS_TRAIN_LBFGS_H__*/
