#ifndef _HUBBARD_HPP_
#define _HUBBARD_HPP_

#include <cstdio>
#include <cstdlib>
#include "simulation_context.h"
#include "k_point.h"
#include "wave_functions.hpp"
#include "non_local_operator.h"
#include "mixer.h"

namespace sirius {

class Hubbard_potential
{

    // Apply Hubbard correction in the colinear case
  private:
    Simulation_context& ctx_;

    Unit_cell& unit_cell_;

    int lmax_{0};

    int number_of_hubbard_orbitals_{0};

    /// Low-frequency mixer for the pseudopotential density mixing.
    std::unique_ptr<Mixer<double_complex>> mixer_{nullptr};

    mdarray<double_complex, 5> occupancy_number_;

    std::vector<int> offset;

    double hubbard_energy_{0.0};
    double hubbard_energy_u_{0.0};
    double hubbard_energy_dc_contribution_{0.0};
    double hubbard_energy_noflip_{0.0};
    double hubbard_energy_flip_{0.0};

    mdarray<double_complex, 5> hubbard_potential_;

    /// type of hubbard correction to be considered.  put to true if we
    /// consider a simple hubbard correction. Not valid if spin orbit
    /// coupling is included
    bool approximation_{false};

    bool orthogonalize_hubbard_orbitals_{false};
    bool normalize_orbitals_only_{true};
    // Hubbard with multi channels
    bool multi_channels_{false};

public:
    void set_hubbard_simple_method(const bool approx)
    {
        approximation_ = approx;
    }

    void set_orthogonalize_hubbard_orbitals(const bool test)
    {
        this->orthogonalize_hubbard_orbitals_ = true;
    }

    void set_normalize_hubbard_orbitals_only(const bool test)
    {
        this->normalize_orbitals_only_ = true;
    }

    double_complex U(int m1, int m2, int m3, int m4) const
    {
        return hubbard_potential_(m1, m2, m3, m4, 0);
    }

    double_complex& U(int m1, int m2, int m3, int m4)
    {
        return hubbard_potential_(m1, m2, m3, m4, 0);
    }

    double_complex U(int m1, int m2, int m3, int m4, int channel) const
    {
        return hubbard_potential_(m1, m2, m3, m4, channel);
    }

    double_complex& U(int m1, int m2, int m3, int m4, int channel)
    {
        return hubbard_potential_(m1, m2, m3, m4, channel);
    }

    const bool& orthogonalize_hubbard_orbitals() const
    {
        return this->orthogonalize_hubbard_orbitals_;
    }

    const bool& normalize_hubbard_orbitals_only() const
    {
        return this->normalize_orbitals_only_;
    }

    void calculate_hubbard_potential_and_energy()
    {
        this->hubbard_energy_                 = 0.0;
        this->hubbard_energy_u_               = 0.0;
        this->hubbard_energy_dc_contribution_ = 0.0;
        this->hubbard_energy_noflip_          = 0.0;
        this->hubbard_energy_flip_            = 0.0;
        // the hubbard potential has the same structure than the occupation
        // numbers
        this->hubbard_potential_.zero();

        if (ctx_.num_mag_dims() != 3) {
            calculate_hubbard_potential_and_energy_colinear_case();
        } else {
            calculate_hubbard_potential_and_energy_non_colinear_case();
        }

        // The potential should be hermitian from the calculations but
        // by security I make it hermitian again

        for (int ia = 0; ia < unit_cell_.num_atoms(); ia++) {
            auto& atom = unit_cell_.atom(ia);
            if (atom.type().hubbard_correction()) {
                // diagonal up up down down blocks
                for (int is = 0; is < ctx_.num_spins(); is++) {
                    for (int m1 = 0; m1 < 2 * atom.type().hubbard_l() + 1; ++m1) {
                        for (int m2 = m1 + 1; m2 < 2 * atom.type().hubbard_l() + 1; ++m2) {
                            this->U(m1, m2, is, ia) = std::conj(this->U(m2, m1, is, ia));
                        }
                    }
                }

                if(ctx_.num_mag_dims() == 3) {
                    for (int m1 = 0; m1 < 2 * atom.type().hubbard_l() + 1; ++m1) {
                        for (int m2 = 0; m2 < 2 * atom.type().hubbard_l() + 1; ++m2) {
                            this->U(m1, m2, 3, ia) = std::conj(this->U(m2, m1, 2, ia));
                        }
                    }
                }
            }
        }
    }

    inline const double hubbard_energy() const
    {
        return this->hubbard_energy_;
    }

    inline const int number_of_hubbard_orbitals() const
    {
        return number_of_hubbard_orbitals_;
    }

    Hubbard_potential(Simulation_context& ctx__)
        : ctx_(ctx__)
        , unit_cell_(ctx__.unit_cell())
    {
        if (!ctx_.hubbard_correction())
            return;
        this->orthogonalize_hubbard_orbitals_ = ctx_.Hubbard().hubbard_orthogonalization_;
        this->normalize_orbitals_only_        = ctx_.Hubbard().hubbard_normalization_;
        int lmax_                             = -1;
        for (int ia = 0; ia < ctx_.unit_cell().num_atoms(); ia++) {
            if (ctx__.unit_cell().atom(ia).type().hubbard_correction()) {
                lmax_ = std::max(lmax_, ctx_.unit_cell().atom(ia).type().hubbard_l());
            }
        }

        /// if spin orbit coupling or non colinear magnetisms are
        /// activated, then we consider the full spherical hubbard
        /// correction
        if ((ctx_.so_correction()) || (ctx_.num_mag_dims() == 3)) {
            approximation_ = false;
        }

        // prepare things for the multi channel case. The last index
        // indicates which channel we consider. By default we only have
        // one channel per atomic type
        occupancy_number_  = mdarray<double_complex, 5>(2 * lmax_ + 1, 2 * lmax_ + 1, 4, ctx_.unit_cell().num_atoms(), 1);
        hubbard_potential_ = mdarray<double_complex, 5>(2 * lmax_ + 1, 2 * lmax_ + 1, 4, ctx_.unit_cell().num_atoms(), 1);

        calculate_wavefunction_with_U_offset();
        calculate_initial_occupation_numbers();

        mixer_ = Mixer_factory<double_complex>(ctx_.mixer_input().type_, static_cast<int>(occupancy_number_.size()), 0,
                                               ctx_.mixer_input(), ctx_.comm());
        this->mixer_input();
        mixer_->initialize();
        calculate_hubbard_potential_and_energy();
    }

    inline void mixer_input()
    {
        for (int i = 0; i < static_cast<int>(occupancy_number_.size()); i++) {
            mixer_->input_shared(i, occupancy_number_[i], 1.0);
        }
    }

    inline void mixer_output()
    {
        for (int i = 0; i < static_cast<int>(occupancy_number_.size()); i++) {
            occupancy_number_[i] = mixer_->output_shared(i);
        }
    }

    double mix()
    {
        double rms;
        mixer_input();
        rms = mixer_->mix(ctx_.settings().mixer_rss_min_);
        mixer_output();
        return rms;
    }

#include "Hubbard/hubbard_generate_atomic_orbitals.hpp"
#include "Hubbard/hubbard_potential_energy.hpp"
#include "Hubbard/apply_hubbard_potential.hpp"
#include "Hubbard/hubbard_occupancy.hpp"
  private:
    inline void calculate_wavefunction_with_U_offset()
    {
        offset.clear();
        offset.resize(ctx_.unit_cell().num_atoms(), -1);

        int counter = 0;
        for (auto ia = 0; ia < unit_cell_.num_atoms(); ia++) {
            auto& atom = unit_cell_.atom(ia);

            if (atom.type().hubbard_correction()) {
                // search for the orbital of given l corresponding to the
                // hubbard l, with strickly positive occupation

                for (size_t wfc = 0; wfc < atom.type().pp_desc().atomic_pseudo_wfs_.size(); wfc++) {
                    int l      = atom.type().pp_desc().atomic_pseudo_wfs_[wfc].first;
                    double occ = atom.type().pp_desc().occupation_wfs[wfc];
                    if ((occ >= 0.0) && (l == atom.type().hubbard_l())) {
                        // a wave function is hubbard if and only if the occupation
                        // number is positive and l corresponds to hubbard_lmax;
                        bool hubbard_wfc = (occ > 0);
                        if (ctx_.num_mag_dims() == 3) {
                            // non colinear case or s.o.

                            // note that for s.o. we should actually compute things
                            // differently since we have j=l+-1/2 (2l or 2l + 2).

                            // problem though right now the computation do not care
                            // that much about j = l +- 1/2 since the orbitals used to
                            // compute the projections are averaged. That's why there
                            // is no switch for spin orbit

                            if (hubbard_wfc && (offset[ia] < 0)) {
                                offset[ia] = counter;
                            }

                            if (hubbard_wfc) {
                                counter += (2 * l + 1);
                            }
                        } else {
                            if (hubbard_wfc) {
                                offset[ia] = counter;
                                // colinear magnetism
                                counter += 2 * l + 1;
                            }
                        }
                    }
                }
            }
        }
        // compute the number of orbitals
        this->number_of_hubbard_orbitals_ = counter;
    }
};
}
#endif