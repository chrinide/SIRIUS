// Copyright (c) 2013-2014 Anton Kozhevnikov, Thomas Schulthess
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that 
// the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the 
//    following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
//    and the following disclaimer in the documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED 
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** \file fft3d.h
 *   
 *  \brief Interface to FFTW3 library.
 */

#ifndef __FFT3D_H__
#define __FFT3D_H__

#include <fftw3.h>
//#include <fftw3-mpi.h>
#include <vector>
#include <algorithm> 
#include "typedefs.h"
#include "mdarray.h"
#include "splindex.h"
#include "vector3d.h"
#include "timer.h"
#include "descriptors.h"
#include "fft3d_grid.h"
#include "gvec.h"

// TODO: allocate and deallocate buffers manually

namespace sirius {

/// Interface to 3D FFT.
/** FFT convention:
 *  \f[
 *      f({\bf r}) = \sum_{{\bf G}} e^{i{\bf G}{\bf r}} f({\bf G})
 *  \f]
 *  is a \em backward transformation from a set of pw coefficients to a function.  
 *
 *  \f[
 *      f({\bf G}) = \frac{1}{\Omega} \int e^{-i{\bf G}{\bf r}} f({\bf r}) d {\bf r} = 
 *          \frac{1}{N} \sum_{{\bf r}_j} e^{-i{\bf G}{\bf r}_j} f({\bf r}_j)
 *  \f]
 *  is a \em forward transformation from a function to a set of coefficients. 
 */
class FFT3D
{
    protected:
        
        /// Number of working threads inside each FFT.
        int num_fft_workers_;
        
        /// Communicator for the parallel FFT.
        Communicator const& comm_;

        /// Main processing unit of this FFT.
        processing_unit_t pu_;
        
        /// Split z-direction.
        splindex<block> spl_z_;

        FFT3D_grid grid_;

        /// Local size of z-dimension of FFT buffer.
        int local_size_z_;

        /// Offset in the global z-dimension.
        int offset_z_;

        /// Main input/output buffer.
        mdarray<double_complex, 1> fft_buffer_;
        
        /// Auxiliary array to store z-sticks for all-to-all or GPU.
        mdarray<double_complex, 1> fft_buffer_aux_;
        
        /// Interbal buffer for independent z-transforms.
        std::vector<double_complex*> fftw_buffer_z_;

        /// Internal buffer for independent {xy}-transforms.
        std::vector<double_complex*> fftw_buffer_xy_;

        std::vector<fftw_plan> plan_backward_z_;

        std::vector<fftw_plan> plan_backward_xy_;
        
        std::vector<fftw_plan> plan_forward_z_;

        std::vector<fftw_plan> plan_forward_xy_;

        #ifdef __GPU
        bool cufft3d_;
        cufftHandle cufft_plan_;
        //cufftHandle cufft_plan_xy_;
        mdarray<char, 1> cufft_work_buf_;
        int cufft_nbatch_;
        #endif

        template <int direction, bool use_reduction>
        void transform_z_serial(Gvec const& gvec__, double_complex* data__);

        template <int direction, bool use_reduction>
        void transform_z_parallel(Gvec const& gvec__, double_complex* data__);

        template <int direction, bool use_reduction>
        void transform_xy(Gvec const& gvec__);

    public:

        FFT3D(FFT3D_grid grid__,
              int num_fft_workers__,
              Communicator const& comm__,
              processing_unit_t pu__,
              double gpu_workload = 0.8);

        ~FFT3D();

        template <int direction>
        void transform(Gvec const& gvec__, double_complex* data__);

        //template<typename T>
        //inline void input(int n__, int const* map__, T const* data__)
        //{
        //    memset(fftw_buffer_, 0, local_size() * sizeof(double_complex));
        //    for (int i = 0; i < n__; i++) fftw_buffer_[map__[i]] = data__[i];
        //}

        template <typename T>
        inline void input(T* data__)
        {
            for (int i = 0; i < local_size(); i++) fft_buffer_[i] = data__[i];
        }
        
        inline void output(double* data__)
        {
            for (int i = 0; i < local_size(); i++) data__[i] = fft_buffer_[i].real();
        }
        
        inline void output(double_complex* data__)
        {
            std::memcpy(data__, &fft_buffer_[0], local_size() * sizeof(double_complex));
        }
        
        //inline void output(int n__, int const* map__, double_complex* data__)
        //{
        //    for (int i = 0; i < n__; i++) data__[i] = fftw_buffer_[map__[i]];
        //}

        //inline void output(int n__, int const* map__, double_complex* data__, double beta__)
        //{
        //    for (int i = 0; i < n__; i++) data__[i] += beta__ * fftw_buffer_[map__[i]];
        //}

        FFT3D_grid const& grid() const
        {
            return grid_;
        }
        
        /// Total size of the FFT grid.
        inline int size() const
        {
            return grid_.size();
        }

        inline int local_size() const
        {
            return grid_.size(0) * grid_.size(1) * local_size_z_;
        }

        inline int local_size_z() const
        {
            return local_size_z_;
        }

        inline int offset_z() const
        {
            return offset_z_;
        }

        /// Direct access to the fft buffer
        inline double_complex& buffer(int idx__)
        {
            return fft_buffer_[idx__];
        }
        
        template <processing_unit_t pu>
        inline double_complex* buffer()
        {
            return fft_buffer_.at<pu>();
        }
        
        Communicator const& comm() const
        {
            return comm_;
        }

        inline bool parallel() const
        {
            return (comm_.size() != 1);
        }

        inline bool hybrid() const
        {
            return (pu_ == GPU);
        }

        inline int num_fft_workers() const
        {
            return num_fft_workers_;
        }

        void allocate_workspace(Gvec const& gvec__)
        {
            /* reallocate auxiliary buffer if needed */
            size_t sz_max;
            if (comm_.size() > 1)
            {
                int rank = comm_.rank();
                int num_zcol_local = gvec__.zcol_fft_distr().counts[rank];
                /* we need this buffer for mpi_alltoall */
                sz_max = std::max(grid_.size(2) * num_zcol_local, local_size());
            }
            else
            {
                sz_max = grid_.size(2) * gvec__.z_columns().size();
            }
            if (sz_max > fft_buffer_aux_.size())
            {
                fft_buffer_aux_ = mdarray<double_complex, 1>(sz_max);
                #ifdef __GPU
                if (pu_ == GPU)
                {
                    fft_buffer_aux_.pin_memory();
                    fft_buffer_aux_.allocate_on_device();
                }
                #endif
            }
            #ifdef __GPU
            if (pu_ == GPU) allocate_on_device();
            #endif
        }

        void deallocate_workspace()
        {
            #ifdef __GPU
            if (pu_ == GPU)
            {
                fft_buffer_aux_.deallocate_on_device();
                deallocate_on_device();
            }
            #endif
        }

        #ifdef __GPU
        void allocate_on_device()
        {
            PROFILE();
            fft_buffer_.pin_memory();
            fft_buffer_.allocate_on_device();
            
            size_t work_size;
            if (comm_.size() == 1 && cufft3d_)
            {
                work_size = cufft_get_size_3d(grid_.size(0), grid_.size(1), grid_.size(2), 1);
            }
            else
            {
                work_size = cufft_get_size_2d(grid_.size(0), grid_.size(1), cufft_nbatch_);
            }
            cufft_work_buf_ = mdarray<char, 1>(nullptr, work_size, "cufft_work_buf_");
            cufft_work_buf_.allocate_on_device();
            cufft_set_work_area(cufft_plan_, cufft_work_buf_.at<GPU>());
        }

        void deallocate_on_device()
        {
            fft_buffer_.unpin_memory();
            fft_buffer_.deallocate_on_device();
            cufft_work_buf_.deallocate_on_device();
        }

        template<typename T>
        inline void input_on_device(int n__, int const* map__, T* data__)
        {
            cufft_batch_load_gpu(local_size(), n__, 1, map__, data__, fft_buffer_.at<GPU>());
        }

        template<typename T>
        inline void output_on_device(int n__, int const* map__, T* data__, double alpha__)
        {
            cufft_batch_unload_gpu(local_size(), n__, 1, map__, fft_buffer_.at<GPU>(), data__, alpha__);
        }
        #endif
};

};

#endif // __FFT3D_H__

/** \page ft_pw Fourier transform and plane wave normalization
 *
 *  We use plane waves in two different cases: a) plane waves (or augmented plane waves in the case of APW+lo method)
 *  form a basis for expanding Kohn-Sham wave functions and b) plane waves are used to expand charge density and
 *  potential. When we are dealing with plane wave basis functions it is convenient to adopt the following 
 *  normalization:
 *  \f[
 *      \langle {\bf r} |{\bf G+k} \rangle = \frac{1}{\sqrt \Omega} e^{i{\bf (G+k)r}}
 *  \f]
 *  such that
 *  \f[
 *      \langle {\bf G+k} |{\bf G'+k} \rangle_{\Omega} = \delta_{{\bf GG'}}
 *  \f]
 *  in the unit cell. However, for the expansion of periodic functions such as density or potential, the following 
 *  convention is more appropriate:
 *  \f[
 *      \rho({\bf r}) = \sum_{\bf G} e^{i{\bf Gr}} \rho({\bf G})
 *  \f]
 *  where
 *  \f[
 *      \rho({\bf G}) = \frac{1}{\Omega} \int_{\Omega} e^{-i{\bf Gr}} \rho({\bf r}) d{\bf r} = 
 *          \frac{1}{\Omega} \sum_{{\bf r}_i} e^{-i{\bf Gr}_i} \rho({\bf r}_i) \frac{\Omega}{N} = 
 *          \frac{1}{N} \sum_{{\bf r}_i} e^{-i{\bf Gr}_i} \rho({\bf r}_i) 
 *  \f]
 *  i.e. with such convention the plane-wave expansion coefficients are obtained with a normalized FFT.
 */

