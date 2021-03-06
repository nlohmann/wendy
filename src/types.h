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

#pragma once

#include <config.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/// the range of hash values (max. 65535)
typedef uint16_t hash_t;

/// the range of labels (max. 255)
typedef uint8_t Label_ID;

/// the range of markings (all markings, deadlock markings) stored within a knowledge (max. 65535)
typedef uint32_t innermarkingcount_t;

/// the range of inner markings (max. 4294967296)
typedef uint32_t InnerMarking_ID;
