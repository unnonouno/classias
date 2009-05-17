/*
 *		Classias library.
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
 *     * Neither the name of the Northwestern University, University of Tokyo,
 *       nor the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
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

/* $Id:$ */

#ifndef __CLASSIAS_CLASSIAS_H__
#define __CLASSIAS_CLASSIAS_H__

#include "base.h"
#include "traits.h"
#include "instance.h"
#include "data.h"

namespace classias
{

typedef feature_data_traits_base<int, int, int> feature_data_traits;
typedef dense_data_traits_base<int, int, int> dense_data_traits;
typedef sparse_data_traits_base<int, int, int> sparse_data_traits;

typedef sparse_vector_base<int, double> sparse_attributes;

typedef binary_instance_base<sparse_attributes, feature_data_traits> binstance;
typedef binary_data_base<binstance, quark> bdata;

typedef candidate_base<sparse_attributes, int> mcandidate;
typedef multi_instance_base<mcandidate, feature_data_traits> minstance;
typedef multi_data_base<minstance, quark, quark> mdata;

typedef attribute_instance_base<sparse_attributes, int, sparse_data_traits> ainstance;
typedef attribute_data_base<ainstance, quark, quark> adata;

typedef attribute_instance_base<sparse_attributes, int, dense_data_traits> dinstance;
typedef attribute_data_base<dinstance, quark, quark> ddata;

};

#endif/*__CLASSIAS_CLASSIAS_H__*/