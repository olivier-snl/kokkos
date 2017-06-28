/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact  H. Carter Edwards (hcedwar@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#ifndef KOKKOS_CUDA_UNIQUE_TOKEN_HPP
#define KOKKOS_CUDA_UNIQUE_TOKEN_HPP

#include <Kokkos_Macros.hpp>
#ifdef KOKKOS_ENABLE_CUDA

#include <Kokkos_CudaSpace.hpp>
#include <Kokkos_UniqueToken.hpp>
#include <impl/Kokkos_SharedAlloc.hpp>
#include <impl/Kokkos_ConcurrentBitset.hpp>

namespace Kokkos { namespace Experimental {

// both global and instance Unique Tokens are implemented in the same way
template< UniqueTokenScope Scope>
class UniqueToken< Cuda, Scope >
{
  using Record = SharedAllocationRecord< CudaSpace >;

  using Tracker = SharedAllocationTracker;

  enum : int32_t { concurrency = 131072 };

  // make this a SharedAllocationRecord
  Tracker  m_track;
  uint32_t volatile * m_buffer;

public:

  using execution_space = Cuda;
  using size_type       = int;

  /// \brief create object size for concurrency on the given instance
  UniqueToken( execution_space const& = execution_space() ) noexcept
  {
    size_t alloc_size = concurrent_bitset::buffer_bound( concurrency );

    Record * record = Record::allocate( CudaSpace(), "UniqueToken", alloc_size );
    m_track.assign_allocated_record_to_uninitialized( record );
    m_buffer = (uint32_t*) record->data();

    uint32_t * tmp = new uint32_t[alloc_size/sizeof(uint32_t)]{};

    cudaMemcpy( m_buffer, tmp, alloc_size, cudaMemcpyDefault );

    delete [] tmp;
  }

  KOKKOS_INLINE_FUNCTION
  UniqueToken( const UniqueToken & ) = default;

  KOKKOS_INLINE_FUNCTION
  UniqueToken( UniqueToken && )      = default;

  KOKKOS_INLINE_FUNCTION
  UniqueToken & operator=( const UniqueToken & ) = default;

  KOKKOS_INLINE_FUNCTION
  UniqueToken & operator=( UniqueToken && )      = default;

  /// \brief upper bound for acquired values, i.e. 0 <= value < size()
  KOKKOS_INLINE_FUNCTION
  int32_t size() const { return concurrency; }

  /// \brief acquire value such that 0 <= value < size()
  KOKKOS_INLINE_FUNCTION
  int32_t acquire() const
  {
    // while loop to aquire
    Kokkos::pair<int,int> result;
    do {
      result = concurrent_bitset::acquire_bounded( buffer
                              , concurrency
                              , Kokkos::Impl::clock_tic() % concurrency
                            //, state
                             );
    } while ( result.second < 0 );

    return result.first;
  }

  /// \brief release a value acquired by generate
  KOKKOS_INLINE_FUNCTION
  void release( int32_t i ) const
  {
    concurrent_bitset::release( buffer, i );
  }
};

}} // namespace Kokkos::Experimental

#endif // KOKKOS_ENABLE_CUDA
#endif // KOKKOS_CUDA_UNIQUE_TOKEN_HPP

