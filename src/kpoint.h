namespace sirius
{

class kpoint
{
    private:

        /// global set of parameters
        Global& parameters_;

        /// weight of k-poitn
        double weight_;

        /// fractional k-point coordinates
        double vk_[3];
        
        /// G+k vectors
        mdarray<double, 2> gkvec_;

        /// global index (in the range [0, N_G - 1]) of G-vector by the index of G+k vector in the range [0, N_Gk - 1]
        std::vector<int> gvec_index_;

        /// first-variational eigen values
        std::vector<double> fv_eigen_values_;

        /// first-variational eigen vectors
        mdarray<complex16, 2> fv_eigen_vectors_;
        
        /// second-variational eigen vectors
        mdarray<complex16, 2> sv_eigen_vectors_;

        /// position of the G vector (from the G+k set) inside the FFT buffer 
        std::vector<int> fft_index_;
       
        /// first-variational states, distributed along the columns of the MPI grid
        mdarray<complex16, 2> fv_states_col_;
       
        /// first-variational states, distributed along the rows of the MPI grid
        mdarray<complex16, 2> fv_states_row_;

        /// two-component (spinor) wave functions describing the bands
        mdarray<complex16, 3> spinor_wave_functions_;

        /// band occupation numbers
        std::vector<double> band_occupancies_;

        /// band energies
        std::vector<double> band_energies_; 

        /// phase factors \f$ e^{i ({\bf G+k}) {\bf r}_{\alpha}} \f$
        mdarray<complex16, 2> gkvec_phase_factors_;

        /// spherical harmonics of G+k vectors
        mdarray<complex16, 2> gkvec_ylm_;

        /// precomputed values for the linear equations for matching coefficients
        mdarray<complex16, 4> alm_b_;

        /// length of G+k vectors
        std::vector<double> gkvec_len_;

        /// number of G+k vectors distributed along rows of MPI grid
        int num_gkvec_row_;
        
        /// number of G+k vectors distributed along columns of MPI grid
        int num_gkvec_col_;

        /// short information about each APW+lo basis function
        std::vector<apwlo_basis_descriptor> apwlo_basis_descriptors_;

        /// row APW+lo basis descriptors
        std::vector<apwlo_basis_descriptor> apwlo_basis_descriptors_row_;
        
        /// column APW+lo basis descriptors
        std::vector<apwlo_basis_descriptor> apwlo_basis_descriptors_col_;
            
        /// list of columns (lo block) for a given atom
        std::vector< std::vector<int> > icol_by_atom_;

        /// list of rows (lo block) for a given atom
        std::vector< std::vector<int> > irow_by_atom_;
        
        inline void generate_matching_coefficients_order1(int ia, int iat, AtomType* type, int l, int num_gkvec_loc, 
                                                          double A, mdarray<complex16, 2>& alm);

        inline void generate_matching_coefficients_order2(int ia, int iat, AtomType* type, int l, int num_gkvec_loc, 
                                                          double R, double A[2][2], mdarray<complex16, 2>& alm);

        /// Generate plane-wave matching coefficents for the radial solutions 
        void generate_matching_coefficients(int num_gkvec_loc, int ia, mdarray<complex16, 2>& alm);
        
        /// Apply the muffin-tin part of the first-variational Hamiltonian to the apw basis function
        
        /** The following vector is computed:
            \f[
              b_{L_2 \nu_2}^{\alpha}({\bf G'}) = \sum_{L_1 \nu_1} \sum_{L_3} 
                a_{L_1\nu_1}^{\alpha*}({\bf G'}) 
                \langle u_{\ell_1\nu_1}^{\alpha} | h_{L3}^{\alpha} |  u_{\ell_2\nu_2}^{\alpha}  
                \rangle  \langle Y_{L_1} | R_{L_3} | Y_{L_2} \rangle +  
                \frac{1}{2} \sum_{\nu_1} a_{L_2\nu_1}^{\alpha *}({\bf G'})
                u_{\ell_2\nu_1}^{\alpha}(R_{\alpha})
                u_{\ell_2\nu_2}^{'\alpha}(R_{\alpha})R_{\alpha}^{2}
            \f] 
        */
        void apply_hmt_to_apw(Band* band, int num_gkvec_row, int ia, mdarray<complex16, 2>& alm, 
                              mdarray<complex16, 2>& halm);

        void check_alm(int num_gkvec_loc, int ia, mdarray<complex16, 2>& alm);
        
        inline void set_fv_h_o_apw_lo(Band* band, AtomType* type, Atom* atom, int ia, int apw_offset_col, 
                                      mdarray<complex16, 2>& alm, mdarray<complex16, 2>& h, mdarray<complex16, 2>& o);

        inline void set_fv_h_o_it(PeriodicFunction<double>* effective_potential, 
                                  mdarray<complex16, 2>& h, mdarray<complex16, 2>& o);

        inline void set_fv_h_o_lo_lo(Band* band, mdarray<complex16, 2>& h, mdarray<complex16, 2>& o);

        /// Setup the Hamiltonian and overlap matrices in APW+lo basis

        /** The Hamiltonian matrix has the following expression:
            \f[
                H_{\mu' \mu} = \langle \varphi_{\mu'} | \hat H | \varphi_{\mu} \rangle
            \f]

            \f[
                H_{\mu' \mu}=\langle \varphi_{\mu' } | \hat H | \varphi_{\mu } \rangle  = 
                \left( \begin{array}{cc} 
                   H_{\bf G'G} & H_{{\bf G'}j} \\
                   H_{j'{\bf G}} & H_{j'j}
                \end{array} \right)
            \f]
            
            The overlap matrix has the following expression:
            \f[
                O_{\mu' \mu} = \langle \varphi_{\mu'} | \varphi_{\mu} \rangle
            \f]
            APW-APW block:
            \f[
                O_{{\bf G'} {\bf G}}^{\bf k} = \sum_{\alpha} \sum_{L\nu} a_{L\nu}^{\alpha *}({\bf G'+k}) 
                a_{L\nu}^{\alpha}({\bf G+k})
            \f]
            
            APW-lo block:
            \f[
                O_{{\bf G'} j}^{\bf k} = \sum_{\nu'} a_{\ell_j m_j \nu'}^{\alpha_j *}({\bf G'+k}) 
                \langle u_{\ell_j \nu'}^{\alpha_j} | \phi_{\ell_j}^{\zeta_j \alpha_j} \rangle
            \f]

            lo-APW block:
            \f[
                O_{j' {\bf G}}^{\bf k} = 
                \sum_{\nu'} \langle \phi_{\ell_{j'}}^{\zeta_{j'} \alpha_{j'}} | u_{\ell_{j'} \nu'}^{\alpha_{j'}} \rangle
                a_{\ell_{j'} m_{j'} \nu'}^{\alpha_{j'}}({\bf G+k}) 
            \f]

            lo-lo block:
            \f[
                O_{j' j}^{\bf k} = \langle \phi_{\ell_{j'}}^{\zeta_{j'} \alpha_{j'}} | 
                \phi_{\ell_{j}}^{\zeta_{j} \alpha_{j}} \rangle \delta_{\alpha_{j'} \alpha_j} 
                \delta_{\ell_{j'} \ell_j} \delta_{m_{j'} m_j}
            \f]

        */
        template <processing_unit_t pu>
        void set_fv_h_o(Band* band, PeriodicFunction<double>* effective_potential, 
                        mdarray<complex16, 2>& h, mdarray<complex16, 2>& o);

        /// Generate first-variational states

        /** 1. setup H and O \n 
            2. solve \$ H\psi = E\psi \$ \n
            3. senerate wave-functions from eigen-vectors

            \param [in] band Pointer to Band class
            \param [in] effective_potential Pointer to effective potential 

        */
        void generate_fv_states(Band* band, PeriodicFunction<double>* effective_potential);

        inline void copy_lo_blocks(const int apwlo_basis_size_row, const int num_gkvec_row, 
                                   const std::vector<apwlo_basis_descriptor>& apwlo_basis_descriptors_row, 
                                   const complex16* z, complex16* vec);
        
        inline void copy_pw_block(const int num_gkvec, const int num_gkvec_row, 
                                  const std::vector<apwlo_basis_descriptor>& apwlo_basis_descriptors_row, 
                                  const complex16* z, complex16* vec);

        void generate_spinor_wave_functions(Band* band, bool has_sv_evec)
        {
            Timer t("sirius::kpoint::generate_spinor_wave_functions");

            spinor_wave_functions_.set_dimensions(mtgk_size(), parameters_.num_spins(), 
                                                  band->spl_spinor_wf_col().local_size());
            if (has_sv_evec)
            {
                spinor_wave_functions_.allocate();
                spinor_wave_functions_.zero();
                
                if (parameters_.num_mag_dims() != 3)
                {
                    for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    {
                        blas<cpu>::gemm(0, 0, mtgk_size(), band->spl_fv_states_col().local_size(), 
                                        band->spl_fv_states_row().local_size(), 
                                        &fv_states_row_(0, 0), fv_states_row_.ld(), 
                                        &sv_eigen_vectors_(0, ispn * band->spl_fv_states_col().local_size()), 
                                        sv_eigen_vectors_.ld(), 
                                        &spinor_wave_functions_(0, ispn, ispn * band->spl_fv_states_col().local_size()), 
                                        spinor_wave_functions_.ld() * parameters_.num_spins());
                    }
                }
                else
                {
                    for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    {
                        blas<cpu>::gemm(0, 0, mtgk_size(), band->spl_spinor_wf_col().local_size(), 
                                        band->num_fv_states_row(ispn), 
                                        &fv_states_row_(0, band->offs_fv_states_row(ispn)), fv_states_row_.ld(), 
                                        &sv_eigen_vectors_(ispn * band->num_fv_states_row_up(), 0), 
                                        sv_eigen_vectors_.ld(), 
                                        &spinor_wave_functions_(0, ispn, 0), 
                                        spinor_wave_functions_.ld() * parameters_.num_spins());
                    }
                }
                
                for (int i = 0; i < band->spl_spinor_wf_col().local_size(); i++)
                {
                    Platform::allreduce(&spinor_wave_functions_(0, 0, i), 
                                        spinor_wave_functions_.size(0) * spinor_wave_functions_.size(1), 
                                        parameters_.mpi_grid().communicator(1 << band->dim_row()));
                }
            }
            else
            {
                spinor_wave_functions_.set_ptr(fv_states_col_.get_ptr());
                memcpy(&band_energies_[0], &fv_eigen_values_[0], parameters_.num_bands() * sizeof(double));
            }
        }

        void generate_gkvec()
        {
            double gk_cutoff = parameters_.aw_cutoff() / parameters_.min_mt_radius();
            
            if (gk_cutoff * parameters_.max_mt_radius() > double(parameters_.lmax_apw()))
            {
                std::stringstream s;
                s << "G+k cutoff (" << gk_cutoff << ") is too large for a given lmax (" 
                  << parameters_.lmax_apw() << ")" << std::endl
                  << "minimum value for lmax : " << int(gk_cutoff * parameters_.max_mt_radius()) + 1;
                error(__FILE__, __LINE__, s);
            }

            if (gk_cutoff * 2 > parameters_.pw_cutoff())
                error(__FILE__, __LINE__, "aw cutoff is too large for a given plane-wave cutoff");

            std::vector< std::pair<double, int> > gkmap;

            // find G-vectors for which |G+k| < cutoff
            for (int ig = 0; ig < parameters_.num_gvec(); ig++)
            {
                double vgk[3];
                for (int x = 0; x < 3; x++) vgk[x] = parameters_.gvec(ig)[x] + vk_[x];

                double v[3];
                parameters_.get_coordinates<cartesian, reciprocal>(vgk, v);
                double gklen = Utils::vector_length(v);

                if (gklen <= gk_cutoff) gkmap.push_back(std::pair<double,int>(gklen, ig));
            }

            std::sort(gkmap.begin(), gkmap.end());

            gkvec_.set_dimensions(3, (int)gkmap.size());
            gkvec_.allocate();

            gvec_index_.resize(gkmap.size());

            for (int ig = 0; ig < (int)gkmap.size(); ig++)
            {
                gvec_index_[ig] = gkmap[ig].second;
                for (int x = 0; x < 3; x++) gkvec_(x, ig) = parameters_.gvec(gkmap[ig].second)[x] + vk_[x];
            }
            
            fft_index_.resize(num_gkvec());
            for (int ig = 0; ig < num_gkvec(); ig++) fft_index_[ig] = parameters_.fft_index(gvec_index_[ig]);
        }

        void init_gkvec()
        {
            gkvec_phase_factors_.set_dimensions(num_gkvec_loc(), parameters_.num_atoms());
            gkvec_phase_factors_.allocate();

            gkvec_ylm_.set_dimensions(parameters_.lmmax_apw(), num_gkvec_loc());
            gkvec_ylm_.allocate();

            #pragma omp parallel for default(shared)
            for (int igkloc = 0; igkloc < num_gkvec_loc(); igkloc++)
            {
                int igk = igkglob(igkloc);
                double v[3];
                double vs[3];

                parameters_.get_coordinates<cartesian, reciprocal>(gkvec(igk), v);
                SHT::spherical_coordinates(v, vs); // vs = {r, theta, phi}

                SHT::spherical_harmonics(parameters_.lmax_apw(), vs[1], vs[2], &gkvec_ylm_(0, igkloc));

                for (int ia = 0; ia < parameters_.num_atoms(); ia++)
                {
                    double phase = twopi * Utils::scalar_product(gkvec(igk), parameters_.atom(ia)->position());

                    gkvec_phase_factors_(igkloc, ia) = exp(complex16(0.0, phase));
                }
            }
            
            alm_b_.set_dimensions(parameters_.lmax_apw() + 1, parameters_.num_atom_types(), num_gkvec_loc(), 2);
            alm_b_.allocate();
            alm_b_.zero();
            
            gkvec_len_.resize(num_gkvec_loc());
            
            // compute values of spherical Bessel functions and first derivative at MT boundary
            mdarray<double, 2> sbessel_mt(parameters_.lmax_apw() + 2, 2);
            sbessel_mt.zero();

            std::vector<complex16> zil(parameters_.lmax_apw() + 1);
            for (int l = 0; l <= parameters_.lmax_apw(); l++) zil[l] = pow(complex16(0, 1), l);
            
            for (int igkloc = 0; igkloc < num_gkvec_loc(); igkloc++)
            {
                int igk = igkglob(igkloc);

                double v[3];
                parameters_.get_coordinates<cartesian, reciprocal>(gkvec(igk), v);
                gkvec_len_[igkloc] = Utils::vector_length(v);
            
                for (int iat = 0; iat < parameters_.num_atom_types(); iat++)
                {
                    double R = parameters_.atom_type(iat)->mt_radius();

                    double gkR = gkvec_len_[igkloc] * R;

                    gsl_sf_bessel_jl_array(parameters_.lmax_apw() + 1, gkR, &sbessel_mt(0, 0));
                    
                    // Bessel function derivative: f_{{n}}^{{\prime}}(z)=-f_{{n+1}}(z)+(n/z)f_{{n}}(z)
                    for (int l = 0; l <= parameters_.lmax_apw(); l++)
                    {
                        sbessel_mt(l, 1) = -sbessel_mt(l + 1, 0) * gkvec_len_[igkloc] + (l / R) * sbessel_mt(l, 0);
                    }
                    
                    for (int l = 0; l <= parameters_.lmax_apw(); l++)
                    {
                        double f = fourpi / sqrt(parameters_.omega());
                        alm_b_(l, iat, igkloc, 0) = zil[l] * f * sbessel_mt(l, 0); 
                        alm_b_(l, iat, igkloc, 1) = zil[l] * f * sbessel_mt(l, 1); 
                    }
                }
            }
        }

        /// Build APW+lo basis descriptors 
        void build_apwlo_basis_descriptors()
        {
            apwlo_basis_descriptor apwlobd;

            // G+k basis functions
            for (int igk = 0; igk < num_gkvec(); igk++)
            {
                apwlobd.igk = igk;
                apwlobd.ig = gvec_index_[igk];
                apwlobd.ia = -1;
                apwlobd.lm = -1;
                apwlobd.l = -1;
                apwlobd.order = -1;
                apwlobd.idxrf = -1;
                apwlobd.idxglob = (int)apwlo_basis_descriptors_.size();
                apwlo_basis_descriptors_.push_back(apwlobd);
            }

            // local orbital basis functions
            for (int ia = 0; ia < parameters_.num_atoms(); ia++)
            {
                Atom* atom = parameters_.atom(ia);
                AtomType* type = atom->type();
            
                int lo_index_offset = type->mt_aw_basis_size();
                
                for (int j = 0; j < type->mt_lo_basis_size(); j++) 
                {
                    int l = type->indexb(lo_index_offset + j).l;
                    int lm = type->indexb(lo_index_offset + j).lm;
                    int order = type->indexb(lo_index_offset + j).order;
                    int idxrf = type->indexb(lo_index_offset + j).idxrf;
                    apwlobd.igk = -1;
                    apwlobd.ig = -1;
                    apwlobd.ia = ia;
                    apwlobd.lm = lm;
                    apwlobd.l = l;
                    apwlobd.order = order;
                    apwlobd.idxrf = idxrf;
                    apwlobd.idxglob = (int)apwlo_basis_descriptors_.size();
                    apwlo_basis_descriptors_.push_back(apwlobd);
                }
            }
            
            // ckeck if we count basis functions correctly
            if ((int)apwlo_basis_descriptors_.size() != (num_gkvec() + parameters_.mt_lo_basis_size()))
                error(__FILE__, __LINE__, "APW+lo basis descriptors array has a wrong size");
        }

        /// Block-cyclic distribution of relevant arrays 
        void distribute_block_cyclic(Band* band)
        {
            // distribute APW+lo basis between rows
            splindex<block_cyclic> spl_row(apwlo_basis_size(), band->num_ranks_row(), band->rank_row(), 
                                           parameters_.cyclic_block_size());
            apwlo_basis_descriptors_row_.resize(spl_row.local_size());
            for (int i = 0; i < spl_row.local_size(); i++)
                apwlo_basis_descriptors_row_[i] = apwlo_basis_descriptors_[spl_row[i]];

            // distribute APW+lo basis between columns
            splindex<block_cyclic> spl_col(apwlo_basis_size(), band->num_ranks_col(), band->rank_col(), 
                                           parameters_.cyclic_block_size());
            apwlo_basis_descriptors_col_.resize(spl_col.local_size());
            for (int i = 0; i < spl_col.local_size(); i++)
                apwlo_basis_descriptors_col_[i] = apwlo_basis_descriptors_[spl_col[i]];
            
            #if defined(_SCALAPACK) || defined(_ELPA_)
            if (parameters_.eigen_value_solver() == scalapack || parameters_.eigen_value_solver() == elpa)
            {
                int nr = linalg<scalapack>::numroc(apwlo_basis_size(), parameters_.cyclic_block_size(), 
                                                   band->rank_row(), 0, band->num_ranks_row());
                
                if (nr != apwlo_basis_size_row()) 
                    error(__FILE__, __LINE__, "numroc returned a different local row size");

                int nc = linalg<scalapack>::numroc(apwlo_basis_size(), parameters_.cyclic_block_size(), 
                                                   band->rank_col(), 0, band->num_ranks_col());
                
                if (nc != apwlo_basis_size_col()) 
                    error(__FILE__, __LINE__, "numroc returned a different local column size");
            }
            #endif

            // get the number of row- and column- G+k-vectors
            num_gkvec_row_ = 0;
            for (int i = 0; i < apwlo_basis_size_row(); i++)
                if (apwlo_basis_descriptors_row_[i].igk != -1) num_gkvec_row_++;
            
            num_gkvec_col_ = 0;
            for (int i = 0; i < apwlo_basis_size_col(); i++)
                if (apwlo_basis_descriptors_col_[i].igk != -1) num_gkvec_col_++;
        }
        
        void test_fv_states(Band* band, int use_fft)
        {
            std::vector<complex16> v1;
            std::vector<complex16> v2;
            
            if (use_fft == 0) 
            {
                v1.resize(num_gkvec());
                v2.resize(parameters_.fft().size());
            }
            
            if (use_fft == 1) 
            {
                v1.resize(parameters_.fft().size());
                v2.resize(parameters_.fft().size());
            }
            
            double maxerr = 0;
        
            for (int j1 = 0; j1 < band->spl_fv_states_col().local_size(); j1++)
            {
                if (use_fft == 0)
                {
                    parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                            &fv_states_col_(parameters_.mt_basis_size(), j1));
                    parameters_.fft().transform(1);
                    parameters_.fft().output(&v2[0]);

                    for (int ir = 0; ir < parameters_.fft().size(); ir++)
                        v2[ir] *= parameters_.step_function(ir);
                    
                    parameters_.fft().input(&v2[0]);
                    parameters_.fft().transform(-1);
                    parameters_.fft().output(num_gkvec(), &fft_index_[0], &v1[0]); 
                }
                
                if (use_fft == 1)
                {
                    parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                            &fv_states_col_(parameters_.mt_basis_size(), j1));
                    parameters_.fft().transform(1);
                    parameters_.fft().output(&v1[0]);
                }
               
                for (int j2 = 0; j2 < band->spl_fv_states_row().local_size(); j2++)
                {
                    complex16 zsum(0.0, 0.0);
                    for (int ia = 0; ia < parameters_.num_atoms(); ia++)
                    {
                        int offset_wf = parameters_.atom(ia)->offset_wf();
                        AtomType* type = parameters_.atom(ia)->type();
                        AtomSymmetryClass* symmetry_class = parameters_.atom(ia)->symmetry_class();
        
                        for (int l = 0; l <= parameters_.lmax_apw(); l++)
                        {
                            int ordmax = type->indexr().num_rf(l);
                            for (int io1 = 0; io1 < ordmax; io1++)
                                for (int io2 = 0; io2 < ordmax; io2++)
                                    for (int m = -l; m <= l; m++)
                                        zsum += conj(fv_states_col_(offset_wf + 
                                                                    type->indexb_by_l_m_order(l, m, io1), j1)) *
                                                     fv_states_row_(offset_wf + 
                                                                    type->indexb_by_l_m_order(l, m, io2), j2) * 
                                                     symmetry_class->o_radial_integral(l, io1, io2);
                        }
                    }
                    
                    if (use_fft == 0)
                    {
                       for (int ig = 0; ig < num_gkvec(); ig++)
                           zsum += conj(v1[ig]) * fv_states_row_(parameters_.mt_basis_size() + ig, j2);
                    }
                   
                    if (use_fft == 1)
                    {
                        parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                           &fv_states_row_(parameters_.mt_basis_size(), j2));
                        parameters_.fft().transform(1);
                        parameters_.fft().output(&v2[0]);
        
                        for (int ir = 0; ir < parameters_.fft().size(); ir++)
                            zsum += conj(v1[ir]) * v2[ir] * parameters_.step_function(ir) / double(parameters_.fft().size());
                    }
                    
                    if (use_fft == 2) 
                    {
                        for (int ig1 = 0; ig1 < num_gkvec(); ig1++)
                        {
                            for (int ig2 = 0; ig2 < num_gkvec(); ig2++)
                            {
                                int ig3 = parameters_.index_g12(gvec_index(ig1), gvec_index(ig2));
                                zsum += conj(fv_states_col_(parameters_.mt_basis_size() + ig1, j1)) * 
                                             fv_states_row_(parameters_.mt_basis_size() + ig2, j2) * 
                                        parameters_.step_function_pw(ig3);
                            }
                       }
                    }

                    if (band->spl_fv_states_col()[j1] == (band->spl_fv_states_row()[j2] % parameters_.num_fv_states()))
                        zsum = zsum - complex16(1, 0);
                   
                    maxerr = std::max(maxerr, abs(zsum));
                }
            }

            Platform::allreduce(&maxerr, 1, 
                                parameters_.mpi_grid().communicator(1 << band->dim_row() | 1 << band->dim_col()));

            if (parameters_.mpi_grid().side(1 << 0)) 
                printf("k-point: %f %f %f, interstitial integration : %i, maximum error : %18.10e\n", 
                       vk_[0], vk_[1], vk_[2], use_fft, maxerr);
        }

#if 0 
        void test_spinor_wave_functions(int use_fft)
        {
            std::vector<complex16> v1[2];
            std::vector<complex16> v2;

            if (use_fft == 0 || use_fft == 1)
                v2.resize(parameters_.fft().size());
            
            if (use_fft == 0) 
                for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    v1[ispn].resize(num_gkvec());
            
            if (use_fft == 1) 
                for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    v1[ispn].resize(parameters_.fft().size());
            
            double maxerr = 0;
        
            for (int j1 = 0; j1 < parameters_.num_bands(); j1++)
            {
                if (use_fft == 0)
                {
                    for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    {
                        parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                           &spinor_wave_functions_(parameters_.mt_basis_size(), ispn, j1));
                        parameters_.fft().transform(1);
                        parameters_.fft().output(&v2[0]);

                        for (int ir = 0; ir < parameters_.fft().size(); ir++)
                            v2[ir] *= parameters_.step_function(ir);
                        
                        parameters_.fft().input(&v2[0]);
                        parameters_.fft().transform(-1);
                        parameters_.fft().output(num_gkvec(), &fft_index_[0], &v1[ispn][0]); 
                    }
                }
                
                if (use_fft == 1)
                {
                    for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    {
                        parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                           &spinor_wave_functions_(parameters_.mt_basis_size(), ispn, j1));
                        parameters_.fft().transform(1);
                        parameters_.fft().output(&v1[ispn][0]);
                    }
                }
               
                for (int j2 = 0; j2 < parameters_.num_bands(); j2++)
                {
                    complex16 zsum(0.0, 0.0);
                    for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                    {
                        for (int ia = 0; ia < parameters_.num_atoms(); ia++)
                        {
                            int offset_wf = parameters_.atom(ia)->offset_wf();
                            AtomType* type = parameters_.atom(ia)->type();
                            AtomSymmetryClass* symmetry_class = parameters_.atom(ia)->symmetry_class();
        
                            for (int l = 0; l <= parameters_.lmax_apw(); l++)
                            {
                                int ordmax = type->indexr().num_rf(l);
                                for (int io1 = 0; io1 < ordmax; io1++)
                                    for (int io2 = 0; io2 < ordmax; io2++)
                                        for (int m = -l; m <= l; m++)
                                            zsum += conj(spinor_wave_functions_(offset_wf + 
                                                                                type->indexb_by_l_m_order(l, m, io1),
                                                                                ispn, j1)) *
                                                         spinor_wave_functions_(offset_wf + 
                                                                                type->indexb_by_l_m_order(l, m, io2), 
                                                                                ispn, j2) * 
                                                         symmetry_class->o_radial_integral(l, io1, io2);
                            }
                        }
                    }
                    
                    if (use_fft == 0)
                    {
                       for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                       {
                           for (int ig = 0; ig < num_gkvec(); ig++)
                               zsum += conj(v1[ispn][ig]) * spinor_wave_functions_(parameters_.mt_basis_size() + ig, ispn, j2);
                       }
                    }
                   
                    if (use_fft == 1)
                    {
                        for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                        {
                            parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                               &spinor_wave_functions_(parameters_.mt_basis_size(), ispn, j2));
                            parameters_.fft().transform(1);
                            parameters_.fft().output(&v2[0]);
        
                            for (int ir = 0; ir < parameters_.fft().size(); ir++)
                                zsum += conj(v1[ispn][ir]) * v2[ir] * parameters_.step_function(ir) / double(parameters_.fft().size());
                        }
                    }
                    
                    if (use_fft == 2) 
                    {
                        for (int ig1 = 0; ig1 < num_gkvec(); ig1++)
                        {
                            for (int ig2 = 0; ig2 < num_gkvec(); ig2++)
                            {
                                int ig3 = parameters_.index_g12(gvec_index(ig1), gvec_index(ig2));
                                for (int ispn = 0; ispn < parameters_.num_spins(); ispn++)
                                    zsum += conj(spinor_wave_functions_(parameters_.mt_basis_size() + ig1, ispn, j1)) * 
                                                 spinor_wave_functions_(parameters_.mt_basis_size() + ig2, ispn, j2) * 
                                            parameters_.step_function_pw(ig3);
                            }
                       }
                   }
       
                   zsum = (j1 == j2) ? zsum - complex16(1.0, 0.0) : zsum;
                   maxerr = std::max(maxerr, abs(zsum));
                }
            }
            std :: cout << "maximum error = " << maxerr << std::endl;
        }
#endif

    public:

        /// Constructor
        kpoint(Global& parameters__, double* vk__, double weight__) : parameters_(parameters__), weight_(weight__)
        {
            for (int x = 0; x < 3; x++) vk_[x] = vk__[x];
        }

        ~kpoint()
        {
        }

        void initialize(Band* band)
        {
            Timer t("sirius::kpoint::initialize");

            generate_gkvec();

            build_apwlo_basis_descriptors();

            distribute_block_cyclic(band);
            
            init_gkvec();
            
            icol_by_atom_.resize(parameters_.num_atoms());
            irow_by_atom_.resize(parameters_.num_atoms());

            for (int icol = num_gkvec_col(); icol < apwlo_basis_size_col(); icol++)
            {
                int ia = apwlo_basis_descriptors_col_[icol].ia;
                icol_by_atom_[ia].push_back(icol);
            }
            
            for (int irow = num_gkvec_row(); irow < apwlo_basis_size_row(); irow++)
            {
                int ia = apwlo_basis_descriptors_row_[irow].ia;
                irow_by_atom_[ia].push_back(irow);
            }
        }

        void find_eigen_states(Band* band, PeriodicFunction<double>* effective_potential, 
                               PeriodicFunction<double>* effective_magnetic_field[3])
        {
            assert(apwlo_basis_size() > parameters_.num_fv_states());
            assert(band != NULL);
            
            Timer t("sirius::kpoint::find_eigen_states");

            generate_fv_states(band, effective_potential);
            
            // distribute fv states along rows of the MPI grid
            if (band->num_ranks() == 1)
            {
                fv_states_row_.set_dimensions(mtgk_size(), parameters_.num_fv_states());
                fv_states_row_.set_ptr(fv_states_col_.get_ptr());
            }
            else
            {
                fv_states_row_.set_dimensions(mtgk_size(), band->spl_fv_states_row().local_size());
                fv_states_row_.allocate();
                fv_states_row_.zero();

                for (int i = 0; i < band->spl_fv_states_row().local_size(); i++)
                {
                    int ist = (band->spl_fv_states_row()[i] % parameters_.num_fv_states());
                    int offset_col = band->spl_fv_states_col().location(0, ist);
                    int rank_col = band->spl_fv_states_col().location(1, ist);
                    if (rank_col == band->rank_col())
                        memcpy(&fv_states_row_(0, i), &fv_states_col_(0, offset_col), mtgk_size() * sizeof(complex16));

                    Platform::allreduce(&fv_states_row_(0, i), mtgk_size(), 
                                        parameters_.mpi_grid().communicator(1 << band->dim_col()));
                }
            }

            if (debug_level > 1) test_fv_states(band, 0);

            sv_eigen_vectors_.set_dimensions(band->spl_fv_states_row().local_size(), 
                                             band->spl_spinor_wf_col().local_size());
            sv_eigen_vectors_.allocate();

            band_energies_.resize(parameters_.num_bands());
            
            if (parameters_.num_spins() == 2) // or some other conditions which require second variation
            {
                band->solve_sv(parameters_, 
                               mtgk_size(), num_gkvec(), fft_index(), &fv_eigen_values_[0], 
                               fv_states_row_, fv_states_col_, effective_magnetic_field, &band_energies_[0],
                               sv_eigen_vectors_);

                generate_spinor_wave_functions(band, true);
            }
            else
            {
                generate_spinor_wave_functions(band, false);
            }

            /*for (int i = 0; i < 3; i++)
                test_spinor_wave_functions(i); */
        }

        PeriodicFunction<complex16>* spinor_wave_function_component(int ispn, int j)
        {
            PeriodicFunction<complex16>* func = new PeriodicFunction<complex16>(parameters_, parameters_.lmax_apw());
            func->allocate(ylm_component | it_component);
            func->zero();

            for (int ia = 0; ia < parameters_.num_atoms(); ia++)
            {
                for (int i = 0; i < parameters_.atom(ia)->type()->mt_basis_size(); i++)
                {
                    int lm = parameters_.atom(ia)->type()->indexb(i).lm;
                    int idxrf = parameters_.atom(ia)->type()->indexb(i).idxrf;
                    for (int ir = 0; ir < parameters_.atom(ia)->num_mt_points(); ir++)
                        func->f_ylm(lm, ir, ia) += 
                            spinor_wave_functions_(parameters_.atom(ia)->offset_wf() + i, ispn, j) * 
                            parameters_.atom(ia)->symmetry_class()->radial_function(ir, idxrf);
                }
            }

            parameters_.fft().input(num_gkvec(), &fft_index_[0], 
                                    &spinor_wave_functions_(parameters_.mt_basis_size(), ispn, j));
            parameters_.fft().transform(1);
            parameters_.fft().output(func->f_it());

            for (int i = 0; i < parameters_.fft().size(); i++)
                func->f_it(i) /= sqrt(parameters_.omega());
            
            return func;
        }
                
        /// Total number of G+k vectors within the cutoff distance
        inline int num_gkvec()
        {
            assert(gkvec_.size(1) == (int)gvec_index_.size());

            return gkvec_.size(1);
        }

        /// Number of G+k vectors along the rows of the matrix
        inline int num_gkvec_row()
        {
            return num_gkvec_row_;
        }

        /// Number of G+k vectors along the columns of the matrix
        inline int num_gkvec_col()
        {
            return num_gkvec_col_;
        }

        /// Local fraction of G+k vectors for a given MPI rank

        /** In case of ScaLAPACK row and column G+k vector blocks are combined. */
        inline int num_gkvec_loc()
        {
            if ((num_gkvec_row() == num_gkvec()) && (num_gkvec_col() == num_gkvec()))
            {
                return num_gkvec();
            }
            else
            {
                return (num_gkvec_row() + num_gkvec_col());
            }
        } 

        inline int igkglob(int igkloc)
        {
            if ((num_gkvec_row() == num_gkvec()) && (num_gkvec_col() == num_gkvec()))
            {
                return igkloc;
            }
            else
            {
                int igk = (igkloc < num_gkvec_row()) ? apwlo_basis_descriptors_row_[igkloc].igk : 
                                                       apwlo_basis_descriptors_col_[igkloc - num_gkvec_row()].igk;
                assert(igk >= 0);
                return igk;
            }
        }
        
        /// Pointer to G+k vector
        inline double* gkvec(int igk)
        {
            assert(igk >= 0 && igk < gkvec_.size(1));

            return &gkvec_(0, igk);
        }

        /// Global index of G-vector by the index of G+k vector
        inline int gvec_index(int igk) 
        {
            assert(igk >= 0 && igk < (int)gvec_index_.size());
            
            return gvec_index_[igk];
        }

        /// APW+lo basis size

        /** Total number of APW+lo basis functions is equal to the number of augmented plane-waves plus
            the number of local orbitals. */
        inline int apwlo_basis_size()
        {
            return (int)apwlo_basis_descriptors_.size();
        }

        /// number of APW+lo basis functions distributed along rows of MPI grid
        inline int apwlo_basis_size_row()
        {
            return (int)apwlo_basis_descriptors_row_.size();
        }

        /// number of APW+lo basis functions distributed along columns of MPI grid
        inline int apwlo_basis_size_col()
        {
            return (int)apwlo_basis_descriptors_col_.size();
        }

        /// Total number of muffin-tin and plane-wave expansion coefficients for the first-variational state

        /** APW+lo basis \f$ \varphi_{\mu {\bf k}}({\bf r}) = \{ \varphi_{\bf G+k}({\bf r}),
            \varphi_{j{\bf k}}({\bf r}) \} \f$ is used to expand first-variational wave-functions:

            \f[
                \psi_{i{\bf k}}({\bf r}) = \sum_{\mu} c_{\mu i}^{\bf k} \varphi_{\mu \bf k}({\bf r}) = 
                \sum_{{\bf G}}c_{{\bf G} i}^{\bf k} \varphi_{\bf G+k}({\bf r}) + 
                \sum_{j}c_{j i}^{\bf k}\varphi_{j{\bf k}}({\bf r})
            \f]

            Inside muffin-tins the expansion is converted into the following form:
            \f[
                \psi_{i {\bf k}}({\bf r})= \begin{array}{ll} 
                \displaystyle \sum_{L} \sum_{\lambda=1}^{N_{\ell}^{\alpha}} 
                F_{L \lambda}^{i {\bf k},\alpha}f_{\ell \lambda}^{\alpha}(r) 
                Y_{\ell m}(\hat {\bf r}) & {\bf r} \in MT_{\alpha} \end{array}
            \f]

            Thus, the total number of coefficients representing a first-variational state is equal
            to the number of muffi-tin basis functions of the form \f$ f_{\ell \lambda}^{\alpha}(r) 
            Y_{\ell m}(\hat {\bf r}) \f$ plust the number of G+k plane waves. 
        */ 
        inline int mtgk_size()
        {
            return (parameters_.mt_basis_size() + num_gkvec());
        }

        inline void get_band_occupancies(double* band_occupancies)
        {
            assert((int)band_occupancies_.size() == parameters_.num_bands());
            
            memcpy(band_occupancies, &band_occupancies_[0], parameters_.num_bands() * sizeof(double));
        }

        inline void set_band_occupancies(double* band_occupancies)
        {
            band_occupancies_.resize(parameters_.num_bands());
            memcpy(&band_occupancies_[0], band_occupancies, parameters_.num_bands() * sizeof(double));
        }

        inline void get_band_energies(double* band_energies)
        {
            assert((int)band_energies_.size() == parameters_.num_bands());
            
            memcpy(band_energies, &band_energies_[0], parameters_.num_bands() * sizeof(double));
        }

        inline void set_band_energies(double* band_energies)
        {
            band_energies_.resize(parameters_.num_bands()); 
            memcpy(&band_energies_[0], band_energies, parameters_.num_bands() * sizeof(double));
        }

        inline double band_occupancy(int j)
        {
            return band_occupancies_[j];
        }
        
        inline double band_energy(int j)
        {
            return band_energies_[j];
        }

        inline double weight()
        {
            return weight_;
        }

        inline complex16& spinor_wave_function(int idxwf, int ispn, int j)
        {
            return spinor_wave_functions_(idxwf, ispn, j);
        }

        inline int* fft_index()
        {
            return &fft_index_[0];
        }

        inline double* vk()
        {
            return vk_;
        }
};

inline void kpoint::generate_matching_coefficients_order1(int ia, int iat, AtomType* type, int l, int num_gkvec_loc, 
                                                          double A, mdarray<complex16, 2>& alm)
{
    if (fabs(A) < 1.0 / sqrt(parameters_.omega()))
    {   
        std::stringstream s;
        s << "Ill defined plane wave matching problem for atom " << ia << ", l = " << l << std::endl
          << "  radial function value at the MT boundary : " << A; 
        
        error(__FILE__, __LINE__, s, fatal_err);
    }
    
    A = 1.0 / A;

    complex16 zt;
    for (int igkloc = 0; igkloc < num_gkvec_loc; igkloc++)
    {
        zt = gkvec_phase_factors_(igkloc, ia) * alm_b_(l, iat, igkloc, 0) * A;

        int idxb = type->indexb_by_l_m_order(l, -l, 0);
        for (int m = -l; m <= l; m++)
        {
            /* it is more convenient to store conjugated coefficients because then the 
               overlap matrix is set with single matrix-matrix multiplication without 
               further conjugation */
            alm(igkloc, idxb++) = gkvec_ylm_(Utils::lm_by_l_m(l, m), igkloc) * conj(zt);
        }
    }
}

inline void kpoint::generate_matching_coefficients_order2(int ia, int iat, AtomType* type, int l, int num_gkvec_loc, 
                                                          double R, double A[2][2], mdarray<complex16, 2>& alm)
{
    double det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    
    if (fabs(det) < 1.0 / sqrt(parameters_.omega()))
    {   
        std::stringstream s;
        s << "Ill defined plane wave matching problem for atom " << ia << ", l = " << l << std::endl
          << "  radial function value at the MT boundary : " << A[0][0]; 
        
        error(__FILE__, __LINE__, s, fatal_err);
    }
    std::swap(A[0][0], A[1][1]);
    A[0][0] /= det;
    A[1][1] /= det;
    A[0][1] = -A[0][1] / det;
    A[1][0] = -A[1][0] / det;
    
    complex16 zt[2];
    complex16 zb[2];
    for (int igkloc = 0; igkloc < num_gkvec_loc; igkloc++)
    {
        double gkR = gkvec_len_[igkloc] * R; // |G+k|*R
        zt[0] = gkvec_phase_factors_(igkloc, ia) * alm_b_(l, iat, igkloc, 0);
        zt[1] = gkR * gkvec_phase_factors_(igkloc, ia) * alm_b_(l, iat, igkloc, 1);

        zb[0] = A[0][0] * zt[0] + A[0][1] * zt[1];
        zb[1] = A[1][0] * zt[0] + A[1][1] * zt[1];

        for (int m = -l; m <= l; m++)
        {
            int idxb0 = type->indexb_by_l_m_order(l, m, 0);
            int idxb1 = type->indexb_by_l_m_order(l, m, 1);
                        
            /* it is more convenient to store conjugated coefficients because then the 
               overlap matrix is set with single matrix-matrix multiplication without 
               further conjugation */
            alm(igkloc, idxb0) = gkvec_ylm_(Utils::lm_by_l_m(l, m), igkloc) * conj(zb[0]);
            alm(igkloc, idxb1) = gkvec_ylm_(Utils::lm_by_l_m(l, m), igkloc) * conj(zb[1]);
        }
    }
}

void kpoint::generate_matching_coefficients(int num_gkvec_loc, int ia, mdarray<complex16, 2>& alm)
{
    Timer t("sirius::kpoint::generate_matching_coefficients");

    Atom* atom = parameters_.atom(ia);
    AtomType* type = atom->type();

    assert(type->max_aw_order() <= 2);

    int iat = parameters_.atom_type_index_by_id(type->id());

    double R = type->mt_radius();

    #pragma omp parallel default(shared)
    {
        double A[2][2];

        #pragma omp for
        for (int l = 0; l <= parameters_.lmax_apw(); l++)
        {
            int num_aw = (int)type->aw_descriptor(l).size();

            for (int order = 0; order < num_aw; order++)
            {
                for (int order1 = 0; order1 < num_aw; order1++)
                {
                    A[order1][order] = atom->symmetry_class()->aw_surface_dm(l, order, order1);
                }
            }

            switch (num_aw)
            {
                case 1:
                {
                    generate_matching_coefficients_order1(ia, iat, type, l, num_gkvec_loc, A[0][0], alm);
                    break;
                }
                case 2:
                {
                    generate_matching_coefficients_order2(ia, iat, type, l, num_gkvec_loc, R, A, alm);
                    break;
                }
                default:
                {
                    error(__FILE__, __LINE__, "wrong order of augmented wave", fatal_err);
                }
            }
        } //l
    }
    
    // check alm coefficients
    if (debug_level > 1) check_alm(num_gkvec_loc, ia, alm);
}

void kpoint::check_alm(int num_gkvec_loc, int ia, mdarray<complex16, 2>& alm)
{
    static SHT sht;
    static bool sht_init = false;
    if (!sht_init)
    {
        sht.set_lmax(parameters_.lmax_apw());
        sht_init = true;
    }

    Atom* atom = parameters_.atom(ia);
    AtomType* type = parameters_.atom(ia)->type();

    mdarray<complex16, 2> z1(sht.num_points(), type->mt_aw_basis_size());
    for (int i = 0; i < type->mt_aw_basis_size(); i++)
    {
        int lm = type->indexb(i).lm;
        int idxrf = type->indexb(i).idxrf;
        double rf = atom->symmetry_class()->radial_function(atom->num_mt_points() - 1, idxrf);
        for (int itp = 0; itp < sht.num_points(); itp++)
        {
            z1(itp, i) = sht.ylm_backward(lm, itp) * rf;
        }
    }

    mdarray<complex16, 2> z2(sht.num_points(), num_gkvec_loc);
    blas<cpu>::gemm(0, 2, sht.num_points(), num_gkvec_loc, type->mt_aw_basis_size(), z1.get_ptr(), z1.ld(),
                    alm.get_ptr(), alm.ld(), z2.get_ptr(), z2.ld());

    double vc[3];
    parameters_.get_coordinates<cartesian, direct>(parameters_.atom(ia)->position(), vc);
    
    double tdiff = 0;
    for (int igloc = 0; igloc < num_gkvec_loc; igloc++)
    {
        double gkc[3];
        parameters_.get_coordinates<cartesian, reciprocal>(gkvec(igkglob(igloc)), gkc);
        for (int itp = 0; itp < sht.num_points(); itp++)
        {
            complex16 aw_value = z2(itp, igloc);
            double r[3];
            for (int x = 0; x < 3; x++) r[x] = vc[x] + sht.coord(x, itp) * type->mt_radius();
            complex16 pw_value = exp(complex16(0, Utils::scalar_product(r, gkc))) / sqrt(parameters_.omega());
            tdiff += abs(pw_value - aw_value);
        }
    }

    printf("atom : %i  absolute alm error : %e  average alm error : %e\n", 
           ia, tdiff, tdiff / (num_gkvec_loc * sht.num_points()));
}

void kpoint::apply_hmt_to_apw(Band* band, int num_gkvec_row, int ia, mdarray<complex16, 2>& alm, 
                              mdarray<complex16, 2>& halm)
{
    Timer t("sirius::kpoint::apply_hmt_to_apw");
    
    Atom* atom = parameters_.atom(ia);
    AtomType* type = atom->type();
    
    #pragma omp parallel default(shared)
    {
        std::vector<complex16> zv(num_gkvec_row);
        
        #pragma omp for
        for (int j2 = 0; j2 < type->mt_aw_basis_size(); j2++)
        {
            memset(&zv[0], 0, num_gkvec_row * sizeof(complex16));

            int lm2 = type->indexb(j2).lm;
            int idxrf2 = type->indexb(j2).idxrf;

            for (int j1 = 0; j1 < type->mt_aw_basis_size(); j1++)
            {
                int lm1 = type->indexb(j1).lm;
                int idxrf1 = type->indexb(j1).idxrf;
                
                complex16 zsum(0, 0);
                
                band->sum_L3_complex_gaunt(lm1, lm2, atom->h_radial_integrals(idxrf1, idxrf2), zsum);
                
                if (abs(zsum) > 1e-14) 
                {
                    for (int ig = 0; ig < num_gkvec_row; ig++) zv[ig] += zsum * alm(ig, j1); 
                }
            } // j1
             
            int l2 = type->indexb(j2).l;
            int order2 = type->indexb(j2).order;
            
            for (int order1 = 0; order1 < (int)type->aw_descriptor(l2).size(); order1++)
            {
                double t1 = 0.5 * pow(type->mt_radius(), 2) * 
                            atom->symmetry_class()->aw_surface_dm(l2, order1, 0) * 
                            atom->symmetry_class()->aw_surface_dm(l2, order2, 1);
                
                for (int ig = 0; ig < num_gkvec_row; ig++) 
                    zv[ig] += t1 * alm(ig, type->indexb_by_lm_order(lm2, order1));
            }
            
            memcpy(&halm(0, j2), &zv[0], num_gkvec_row * sizeof(complex16));

        } // j2
    }
}

inline void kpoint::set_fv_h_o_apw_lo(Band* band, AtomType* type, Atom* atom, int ia, int apw_offset_col, 
                                      mdarray<complex16, 2>& alm, mdarray<complex16, 2>& h, mdarray<complex16, 2>& o)
{
    Timer t("sirius::kpoint::set_fv_h_o_apw_lo");
    
    // apw-lo block
    for (int i = 0; i < (int)icol_by_atom_[ia].size(); i++)
    {
        int icol = icol_by_atom_[ia][i];

        int l = apwlo_basis_descriptors_col_[icol].l;
        int lm = apwlo_basis_descriptors_col_[icol].lm;
        int idxrf = apwlo_basis_descriptors_col_[icol].idxrf;
        int order = apwlo_basis_descriptors_col_[icol].order;
        
        for (int j1 = 0; j1 < type->mt_aw_basis_size(); j1++) 
        {
            int lm1 = type->indexb(j1).lm;
            int idxrf1 = type->indexb(j1).idxrf;
                    
            complex16 zsum(0, 0);
                    
            band->sum_L3_complex_gaunt(lm1, lm, atom->h_radial_integrals(idxrf, idxrf1), zsum);

            if (abs(zsum) > 1e-14)
            {
                for (int igkloc = 0; igkloc < num_gkvec_row(); igkloc++)
                    h(igkloc, icol) += zsum * alm(igkloc, j1);
            }
        }

        for (int order1 = 0; order1 < (int)type->aw_descriptor(l).size(); order1++)
        {
            for (int igkloc = 0; igkloc < num_gkvec_row(); igkloc++)
            {
                o(igkloc, icol) += atom->symmetry_class()->o_radial_integral(l, order1, order) * 
                                   alm(igkloc, type->indexb_by_lm_order(lm, order1));
            }
        }
    }

    std::vector<complex16> ztmp(num_gkvec_col());
    // lo-apw block
    for (int i = 0; i < (int)irow_by_atom_[ia].size(); i++)
    {
        int irow = irow_by_atom_[ia][i];

        int l = apwlo_basis_descriptors_row_[irow].l;
        int lm = apwlo_basis_descriptors_row_[irow].lm;
        int idxrf = apwlo_basis_descriptors_row_[irow].idxrf;
        int order = apwlo_basis_descriptors_row_[irow].order;

        memset(&ztmp[0], 0, num_gkvec_col() * sizeof(complex16));
    
        for (int j1 = 0; j1 < type->mt_aw_basis_size(); j1++) 
        {
            int lm1 = type->indexb(j1).lm;
            int idxrf1 = type->indexb(j1).idxrf;
                    
            complex16 zsum(0, 0);
                    
            band->sum_L3_complex_gaunt(lm, lm1, atom->h_radial_integrals(idxrf, idxrf1), zsum);

            if (abs(zsum) > 1e-14)
            {
                for (int igkloc = 0; igkloc < num_gkvec_col(); igkloc++)
                    ztmp[igkloc] += zsum * conj(alm(apw_offset_col + igkloc, j1));
            }
        }

        for (int igkloc = 0; igkloc < num_gkvec_col(); igkloc++) h(irow, igkloc) += ztmp[igkloc]; 

        for (int order1 = 0; order1 < (int)type->aw_descriptor(l).size(); order1++)
        {
            for (int igkloc = 0; igkloc < num_gkvec_col(); igkloc++)
            {
                o(irow, igkloc) += atom->symmetry_class()->o_radial_integral(l, order, order1) * 
                                   conj(alm(apw_offset_col + igkloc, type->indexb_by_lm_order(lm, order1)));
            }
        }
    }
}

inline void kpoint::set_fv_h_o_it(PeriodicFunction<double>* effective_potential, 
                                  mdarray<complex16, 2>& h, mdarray<complex16, 2>& o)
{
    Timer t("sirius::kpoint::set_fv_h_o_it");

    #pragma omp parallel for default(shared)
    for (int igkloc2 = 0; igkloc2 < num_gkvec_col(); igkloc2++) // loop over columns
    {
        double v2c[3];
        parameters_.get_coordinates<cartesian, reciprocal>(gkvec(apwlo_basis_descriptors_col_[igkloc2].igk), v2c);

        for (int igkloc1 = 0; igkloc1 < num_gkvec_row(); igkloc1++) // for each column loop over rows
        {
            int ig12 = parameters_.index_g12(apwlo_basis_descriptors_row_[igkloc1].ig,
                                             apwlo_basis_descriptors_col_[igkloc2].ig);
            double v1c[3];
            parameters_.get_coordinates<cartesian, reciprocal>(gkvec(apwlo_basis_descriptors_row_[igkloc1].igk), v1c);
            
            double t1 = 0.5 * Utils::scalar_product(v1c, v2c);
                               
            h(igkloc1, igkloc2) += (effective_potential->f_pw(ig12) + t1 * parameters_.step_function_pw(ig12));
            o(igkloc1, igkloc2) += parameters_.step_function_pw(ig12);
        }
    }
}

inline void kpoint::set_fv_h_o_lo_lo(Band* band, mdarray<complex16, 2>& h, mdarray<complex16, 2>& o)
{
    Timer t("sirius::kpoint::set_fv_h_o_lo_lo");

    // lo-lo block
    #pragma omp parallel for default(shared)
    for (int icol = num_gkvec_col(); icol < apwlo_basis_size_col(); icol++)
    {
        int ia = apwlo_basis_descriptors_col_[icol].ia;
        int lm2 = apwlo_basis_descriptors_col_[icol].lm; 
        int idxrf2 = apwlo_basis_descriptors_col_[icol].idxrf; 

        for (int irow = num_gkvec_row(); irow < apwlo_basis_size_row(); irow++)
        {
            if (ia == apwlo_basis_descriptors_row_[irow].ia)
            {
                Atom* atom = parameters_.atom(ia);
                int lm1 = apwlo_basis_descriptors_row_[irow].lm; 
                int idxrf1 = apwlo_basis_descriptors_row_[irow].idxrf; 

                complex16 zsum(0, 0);

                band->sum_L3_complex_gaunt(lm1, lm2, atom->h_radial_integrals(idxrf1, idxrf2), zsum);

                h(irow, icol) += zsum;
                
                if (lm1 == lm2)
                {
                    int l = apwlo_basis_descriptors_row_[irow].l;

                    int order1 = apwlo_basis_descriptors_row_[irow].order; 
                    int order2 = apwlo_basis_descriptors_col_[icol].order; 
                    o(irow, icol) += atom->symmetry_class()->o_radial_integral(l, order1, order2);
                }
            }
        }
    }
}

template<> void kpoint::set_fv_h_o<cpu>(Band* band, PeriodicFunction<double>* effective_potential, 
                                        mdarray<complex16, 2>& h, mdarray<complex16, 2>& o)

{
    Timer t("sirius::kpoint::set_fv_h_o");
    
    int apw_offset_col = (band->num_ranks() > 1) ? num_gkvec_row() : 0;
    
    mdarray<complex16, 2> alm(NULL, num_gkvec_loc(), parameters_.max_mt_aw_basis_size());
    mdarray<complex16, 2> halm(NULL, num_gkvec_row(), parameters_.max_mt_aw_basis_size());

    alm.allocate();
    halm.allocate();
    h.zero();
    o.zero();

    complex16 zone(1, 0);
    
    for (int ia = 0; ia < parameters_.num_atoms(); ia++)
    {
        Atom* atom = parameters_.atom(ia);
        AtomType* type = atom->type();
        
        generate_matching_coefficients(num_gkvec_loc(), ia, alm);
        
        apply_hmt_to_apw(band, num_gkvec_row(), ia, alm, halm);
        
        blas<cpu>::gemm(0, 2, num_gkvec_row(), num_gkvec_col(), type->mt_aw_basis_size(), zone, &alm(0, 0), alm.ld(), 
                        &alm(apw_offset_col, 0), alm.ld(), zone, &o(0, 0), o.ld()); 
            
        // apw-apw block
        blas<cpu>::gemm(0, 2, num_gkvec_row(), num_gkvec_col(), type->mt_aw_basis_size(), zone, &halm(0, 0), halm.ld(), 
                        &alm(apw_offset_col, 0), alm.ld(), zone, &h(0, 0), h.ld());
        
        set_fv_h_o_apw_lo(band, type, atom, ia, apw_offset_col, alm, h, o);
    } //ia

    set_fv_h_o_it(effective_potential, h, o);

    set_fv_h_o_lo_lo(band, h, o);

    alm.deallocate();
    halm.deallocate();
}

#ifdef _GPU_
template<> void kpoint::set_fv_h_o<gpu>(Band* band, PeriodicFunction<double>* effective_potential, 
                                        mdarray<complex16, 2>& h, mdarray<complex16, 2>& o)

{
    Timer t("sirius::kpoint::set_fv_h_o");
    
    int apw_offset_col = (band->num_ranks() > 1) ? num_gkvec_row() : 0;
    
    mdarray<complex16, 2> alm(NULL, num_gkvec_loc(), parameters_.max_mt_aw_basis_size());
    mdarray<complex16, 2> halm(NULL, num_gkvec_row(), parameters_.max_mt_aw_basis_size());
    
    alm.allocate();
    alm.pin_memory();
    alm.allocate_on_device();
    halm.allocate();
    halm.pin_memory();
    halm.allocate_on_device();
    h.allocate_on_device();
    h.zero_on_device();
    o.allocate_on_device();
    o.zero_on_device();

    complex16 zone(1, 0);
    
    for (int ia = 0; ia < parameters_.num_atoms(); ia++)
    {
        Atom* atom = parameters_.atom(ia);
        AtomType* type = atom->type();
        
        generate_matching_coefficients(num_gkvec_loc(), ia, alm);
        
        apply_hmt_to_apw(band, num_gkvec_row(), ia, alm, halm);
        
        alm.copy_to_device();
        halm.copy_to_device();
        
        blas<gpu>::gemm(0, 2, num_gkvec_row(), num_gkvec_col(), type->mt_aw_basis_size(), &zone, 
                        alm.get_ptr_device(), alm.ld(), &(alm.get_ptr_device()[apw_offset_col]), alm.ld(), 
                        &zone, o.get_ptr_device(), o.ld()); 
        
        blas<gpu>::gemm(0, 2, num_gkvec_row(), num_gkvec_col(), type->mt_aw_basis_size(), &zone, 
                        halm.get_ptr_device(), halm.ld(), &(alm.get_ptr_device()[apw_offset_col]), alm.ld(),
                        &zone, h.get_ptr_device(), h.ld());
            
        set_fv_h_o_apw_lo(band, type, atom, ia, apw_offset_col, alm, h, o);
    } //ia

    cublas_get_matrix(num_gkvec_row(), num_gkvec_col(), sizeof(complex16), h.get_ptr_device(), h.ld(), 
                      h.get_ptr(), h.ld());
    
    cublas_get_matrix(num_gkvec_row(), num_gkvec_col(), sizeof(complex16), o.get_ptr_device(), o.ld(), 
                      o.get_ptr(), o.ld());

    set_fv_h_o_it(effective_potential, h, o);

    set_fv_h_o_lo_lo(band, h, o);

    h.deallocate_on_device();
    o.deallocate_on_device();
    alm.deallocate_on_device();
    alm.unpin_memory();
    alm.deallocate();
    halm.deallocate_on_device();
    halm.unpin_memory();
    halm.deallocate();
}
#endif

inline void kpoint::copy_lo_blocks(const int apwlo_basis_size_row, const int num_gkvec_row, 
                                   const std::vector<apwlo_basis_descriptor>& apwlo_basis_descriptors_row, 
                                   const complex16* z, complex16* vec)
{
    for (int j = num_gkvec_row; j < apwlo_basis_size_row; j++)
    {
        int ia = apwlo_basis_descriptors_row[j].ia;
        int lm = apwlo_basis_descriptors_row[j].lm;
        int order = apwlo_basis_descriptors_row[j].order;
        vec[parameters_.atom(ia)->offset_wf() + parameters_.atom(ia)->type()->indexb_by_lm_order(lm, order)] = z[j];
    }
}

inline void kpoint::copy_pw_block(const int num_gkvec, const int num_gkvec_row, 
                                  const std::vector<apwlo_basis_descriptor>& apwlo_basis_descriptors_row, 
                                  const complex16* z, complex16* vec)
{
    memset(vec, 0, num_gkvec * sizeof(complex16));

    for (int j = 0; j < num_gkvec_row; j++) vec[apwlo_basis_descriptors_row[j].igk] = z[j];
}

void kpoint::generate_fv_states(Band* band, PeriodicFunction<double>* effective_potential)
{
    Timer t("sirius::kpoint::generate_fv_states");

    mdarray<complex16, 2> h(NULL, apwlo_basis_size_row(), apwlo_basis_size_col());
    mdarray<complex16, 2> o(NULL, apwlo_basis_size_row(), apwlo_basis_size_col());
    
    // Magma requires special allocation
    h.allocate();
    o.allocate();
    #ifdef _MAGMA_
    if (parameters_.eigen_value_solver() == magma)
    {
        h.pin_memory();
        o.pin_memory();
    }
    #endif
   
    // setup Hamiltonian and overlap
    switch (parameters_.processing_unit())
    {
        case cpu:
        {
            set_fv_h_o<cpu>(band, effective_potential, h, o);
            break;
        }
        #ifdef _GPU_
        case gpu:
        {
            set_fv_h_o<gpu>(band, effective_potential, h, o);
            break;
        }
        #endif
        default:
        {
            error(__FILE__, __LINE__, "wrong processing unit");
        }
    }

    //sirius_io::hdf5_write_matrix("h.h5", h);
    //sirius_io::hdf5_write_matrix("o.h5", o);
    //Utils::write_matrix("h.txt", true, h);
    //Utils::write_matrix("o.txt", true, o);

    if ((debug_level > 0) && (parameters_.eigen_value_solver() == lapack))
    {
        standard_evp_lapack s;

        std::vector<double> o_eval(apwlo_basis_size());
        mdarray<complex16, 2> o_tmp(apwlo_basis_size(), apwlo_basis_size());
        memcpy(o_tmp.get_ptr(), o.get_ptr(), o.size() * sizeof(complex16));
        mdarray<complex16, 2> o_evec(apwlo_basis_size(), apwlo_basis_size());
    
        s.solve(apwlo_basis_size(), o_tmp.get_ptr(), o_tmp.ld(), &o_eval[0], o_evec.get_ptr(), o_evec.ld());
        for (int i = 0; i < apwlo_basis_size(); i++)
        {
            if (o_eval[i] < 1e-7)
            {
                //Utils::write_matrix("o.txt", true, o);
                std::stringstream ss;
                
                //for (int irow = num_gkvec_row(); irow < apwlo_basis_size_row(); irow++)
                //{
                //    for (int icol = num_gkvec_col(); icol < apwlo_basis_size_col(); icol++)
                //    {
                //        printf("%12.6f ", abs(o(irow, icol)));
                //    }
                //    printf("\n");
                //}

                ss << "ill defined overlap matrix" << std::endl;
                for (int j = 0; j < apwlo_basis_size(); j++) ss << o_eval[j] << " ";
                error(__FILE__, __LINE__, ss, 0);
            }
        }
    }

    if ((debug_level > 0) && (parameters_.eigen_value_solver() == lapack))
    {
        Utils::check_hermitian("h", h);
        Utils::check_hermitian("o", o);
    }

    if (verbosity_level > 1)
    {
        double h_max = 0;
        double o_max = 0;
        int h_irow = 0;
        int h_icol = 0;
        int o_irow = 0;
        int o_icol = 0;
        std::vector<double> h_diag(apwlo_basis_size(), 0);
        std::vector<double> o_diag(apwlo_basis_size(), 0);
        for (int icol = 0; icol < apwlo_basis_size_col(); icol++)
        {
            int idxglob = apwlo_basis_descriptors_col_[icol].idxglob;
            for (int irow = 0; irow < apwlo_basis_size_row(); irow++)
            {
                if (apwlo_basis_descriptors_row_[irow].idxglob == idxglob)
                {
                    h_diag[idxglob] = abs(h(irow, icol));
                    o_diag[idxglob] = abs(o(irow, icol));
                }
                if (abs(h(irow, icol)) > h_max)
                {
                    h_max = abs(h(irow, icol));
                    h_irow = irow;
                    h_icol = icol;
                }
                if (abs(o(irow, icol)) > o_max)
                {
                    o_max = abs(o(irow, icol));
                    o_irow = irow;
                    o_icol = icol;
                }
            }
        }

        Platform::allreduce(&h_diag[0], apwlo_basis_size(),
                            parameters_.mpi_grid().communicator(1 << band->dim_row() | 1 << band->dim_col()));
        
        Platform::allreduce(&o_diag[0], apwlo_basis_size(),
                            parameters_.mpi_grid().communicator(1 << band->dim_row() | 1 << band->dim_col()));
        
        if (parameters_.mpi_grid().root(1 << band->dim_row() | 1 << band->dim_col()))
        {
            std::stringstream s;
            s << "h_diag : ";
            for (int i = 0; i < apwlo_basis_size(); i++) s << h_diag[i] << " ";
            s << std::endl;
            s << "o_diag : ";
            for (int i = 0; i < apwlo_basis_size(); i++) s << o_diag[i] << " ";
            warning(__FILE__, __LINE__, s, 0);
        }

        std::stringstream s;
        s << "h_max " << h_max << " irow, icol : " << h_irow << " " << h_icol << std::endl;
        s << " (row) igk, ig, ia, l, lm, irdrf, order : " << apwlo_basis_descriptors_row_[h_irow].igk << " "  
                                                          << apwlo_basis_descriptors_row_[h_irow].ig << " "  
                                                          << apwlo_basis_descriptors_row_[h_irow].ia << " "  
                                                          << apwlo_basis_descriptors_row_[h_irow].l << " "  
                                                          << apwlo_basis_descriptors_row_[h_irow].lm << " "  
                                                          << apwlo_basis_descriptors_row_[h_irow].idxrf << " "  
                                                          << apwlo_basis_descriptors_row_[h_irow].order 
                                                          << std::endl;
        s << " (col) igk, ig, ia, l, lm, irdrf, order : " << apwlo_basis_descriptors_col_[h_icol].igk << " "  
                                                          << apwlo_basis_descriptors_col_[h_icol].ig << " "  
                                                          << apwlo_basis_descriptors_col_[h_icol].ia << " "  
                                                          << apwlo_basis_descriptors_col_[h_icol].l << " "  
                                                          << apwlo_basis_descriptors_col_[h_icol].lm << " "  
                                                          << apwlo_basis_descriptors_col_[h_icol].idxrf << " "  
                                                          << apwlo_basis_descriptors_col_[h_icol].order 
                                                          << std::endl;

        s << "o_max " << o_max << " irow, icol : " << o_irow << " " << o_icol << std::endl;
        s << " (row) igk, ig, ia, l, lm, irdrf, order : " << apwlo_basis_descriptors_row_[o_irow].igk << " "  
                                                          << apwlo_basis_descriptors_row_[o_irow].ig << " "  
                                                          << apwlo_basis_descriptors_row_[o_irow].ia << " "  
                                                          << apwlo_basis_descriptors_row_[o_irow].l << " "  
                                                          << apwlo_basis_descriptors_row_[o_irow].lm << " "  
                                                          << apwlo_basis_descriptors_row_[o_irow].idxrf << " "  
                                                          << apwlo_basis_descriptors_row_[o_irow].order 
                                                          << std::endl;
        s << " (col) igk, ig, ia, l, lm, irdrf, order : " << apwlo_basis_descriptors_col_[o_icol].igk << " "  
                                                          << apwlo_basis_descriptors_col_[o_icol].ig << " "  
                                                          << apwlo_basis_descriptors_col_[o_icol].ia << " "  
                                                          << apwlo_basis_descriptors_col_[o_icol].l << " "  
                                                          << apwlo_basis_descriptors_col_[o_icol].lm << " "  
                                                          << apwlo_basis_descriptors_col_[o_icol].idxrf << " "  
                                                          << apwlo_basis_descriptors_col_[o_icol].order 
                                                          << std::endl;
        warning(__FILE__, __LINE__, s, 0);
    }
    
    fv_eigen_values_.resize(parameters_.num_fv_states());

    fv_eigen_vectors_.set_dimensions(apwlo_basis_size_row(), band->spl_fv_states_col().local_size());
    fv_eigen_vectors_.allocate();
   
    // debug scalapack
    std::vector<double> fv_eigen_values_glob(parameters_.num_fv_states());
    if ((debug_level > 2) && 
        (parameters_.eigen_value_solver() == scalapack || parameters_.eigen_value_solver() == elpa))
    {
        mdarray<complex16, 2> h_glob(apwlo_basis_size(), apwlo_basis_size());
        mdarray<complex16, 2> o_glob(apwlo_basis_size(), apwlo_basis_size());
        mdarray<complex16, 2> fv_eigen_vectors_glob(apwlo_basis_size(), parameters_.num_fv_states());

        h_glob.zero();
        o_glob.zero();

        for (int icol = 0; icol < apwlo_basis_size_col(); icol++)
        {
            int j = apwlo_basis_descriptors_col_[icol].idxglob;
            for (int irow = 0; irow < apwlo_basis_size_row(); irow++)
            {
                int i = apwlo_basis_descriptors_row_[irow].idxglob;
                h_glob(i, j) = h(irow, icol);
                o_glob(i, j) = o(irow, icol);
            }
        }
        
        Platform::allreduce(h_glob.get_ptr(), (int)h_glob.size(), 
                            parameters_.mpi_grid().communicator(1 << band->dim_row() | 1 << band->dim_col()));
        
        Platform::allreduce(o_glob.get_ptr(), (int)o_glob.size(), 
                            parameters_.mpi_grid().communicator(1 << band->dim_row() | 1 << band->dim_col()));

        Utils::check_hermitian("h_glob", h_glob);
        Utils::check_hermitian("o_glob", o_glob);
        
        generalized_evp_lapack lapack_solver(-1.0);

        lapack_solver.solve(apwlo_basis_size(), parameters_.num_fv_states(), h_glob.get_ptr(), h_glob.ld(), 
                            o_glob.get_ptr(), o_glob.ld(), &fv_eigen_values_glob[0], fv_eigen_vectors_glob.get_ptr(),
                            fv_eigen_vectors_glob.ld());
    }
    
    Timer *t1 = new Timer("sirius::kpoint::generate_fv_states:genevp");
    generalized_evp* solver = NULL;

    switch (parameters_.eigen_value_solver())
    {
        case lapack:
        {

            solver = new generalized_evp_lapack(-1.0);
            break;
        }
        case scalapack:
        {

            solver = new generalized_evp_scalapack(parameters_.cyclic_block_size(), band->num_ranks_row(), 
                                                   band->num_ranks_col(), band->blacs_context(), -1.0);
            break;
        }
        case elpa:
        {

            solver = new generalized_evp_elpa(parameters_.cyclic_block_size(), apwlo_basis_size_row(), 
                                              band->num_ranks_row(), band->rank_row(),
                                              apwlo_basis_size_col(), band->num_ranks_col(), 
                                              band->rank_col(), band->blacs_context(), 
                                              parameters_.mpi_grid().communicator(1 << band->dim_row()),
                                              parameters_.mpi_grid().communicator(1 << band->dim_col()),
                                              parameters_.mpi_grid().communicator(1 << band->dim_col() | 
                                                                                  1 << band->dim_row()));
            break;
        }
        case magma:
        {
            solver = new generalized_evp_magma();
            break;
        }
        default:
        {
            error(__FILE__, __LINE__, "eigen value solver is not defined", fatal_err);
        }
    }

    solver->solve(apwlo_basis_size(), parameters_.num_fv_states(), h.get_ptr(), h.ld(), o.get_ptr(), o.ld(), 
                  &fv_eigen_values_[0], fv_eigen_vectors_.get_ptr(), fv_eigen_vectors_.ld());

    delete solver;

    delete t1;
   
    #ifdef _MAGMA_
    if (parameters_.eigen_value_solver() == magma)
    {
        h.unpin_memory();
        o.unpin_memory();
    }
    #endif
   
    h.deallocate();
    o.deallocate();
    
    if ((debug_level > 2) && (parameters_.eigen_value_solver() == scalapack))
    {
        double d = 0.0;
        for (int i = 0; i < parameters_.num_fv_states(); i++) 
            d += fabs(fv_eigen_values_[i] - fv_eigen_values_glob[i]);
        std::stringstream s;
        s << "Totoal eigen-value difference : " << d;
        warning(__FILE__, __LINE__, s, 0);
    }
    
    // generate first-variational wave-functions
    fv_states_col_.set_dimensions(mtgk_size(), band->spl_fv_states_col().local_size());
    fv_states_col_.allocate();
    fv_states_col_.zero();

    mdarray<complex16, 2> alm(num_gkvec_row(), parameters_.max_mt_aw_basis_size());
    
    Timer *t2 = new Timer("sirius::kpoint::generate_fv_states:wf");
    for (int ia = 0; ia < parameters_.num_atoms(); ia++)
    {
        Atom* atom = parameters_.atom(ia);
        AtomType* type = atom->type();
        
        generate_matching_coefficients(num_gkvec_row(), ia, alm);

        blas<cpu>::gemm(2, 0, type->mt_aw_basis_size(), band->spl_fv_states_col().local_size(),
                        num_gkvec_row(), &alm(0, 0), alm.ld(), &fv_eigen_vectors_(0, 0), 
                        fv_eigen_vectors_.ld(), &fv_states_col_(atom->offset_wf(), 0), 
                        fv_states_col_.ld());
    }

    for (int j = 0; j < band->spl_fv_states_col().local_size(); j++)
    {
        copy_lo_blocks(apwlo_basis_size_row(), num_gkvec_row(), apwlo_basis_descriptors_row_, 
                       &fv_eigen_vectors_(0, j), &fv_states_col_(0, j));

        copy_pw_block(num_gkvec(), num_gkvec_row(), apwlo_basis_descriptors_row_, 
                      &fv_eigen_vectors_(0, j), &fv_states_col_(parameters_.mt_basis_size(), j));
    }

    for (int j = 0; j < band->spl_fv_states_col().local_size(); j++)
    {
        Platform::allreduce(&fv_states_col_(0, j), mtgk_size(), 
                            parameters_.mpi_grid().communicator(1 << band->dim_row()));
    }
    delete t2;

    fv_eigen_vectors_.deallocate();
}

};

