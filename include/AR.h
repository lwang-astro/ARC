#pragma once

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cmath>
//#include <cstdlib>
//#include <cstdio>
#include <limits>
#include <typeinfo>
#include <typeindex>
#ifdef USE_OMP
#include <omp.h>
#endif
#ifdef ARC_DEBUG
#include <cassert>
#endif

#ifdef FAPP_PROFILE
#include <fj_tool/fapp.h>
#endif

#ifdef ARC_DEEP_DEBUG
#define WIDTH 10
#endif

#include "Float.h"
#include "extrapolation.h"
#include "symplectic.h"

//! Algorithmic regularization chain (ARC) namespace
/*!
  All major ARC classes and related acceleration functions (typedef) are defined
 */
namespace ARC {

typedef Float Float3[3];
typedef Float Float2[2];
    
#ifdef ARC_PROFILE
#include <sys/time.h>
static Float get_wtime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + 1.e-6 * tv.tv_usec;
}

//! A structure storing time profile 
class chainprofile{
private:
  int  itermax;   ///< maximum iter level
public:
  Float t_apw;    ///< APW calculation
  Float t_uplink; ///< update link
  Float t_lf;     ///< leap-frog
  Float t_ep;     ///< extrapolation
  Float t_pext;   ///< perturber force
  Float t_dense;  ///< dense output
  Float t_init;   ///< initialization
  Float t_newdt;  ///< new timestep
  Float t_check;  ///< for debug
  Float t_sym;    ///< symplectic_integration

  int  itercount;  ///< total substep in extrapolation
  int* stepcount;  ///< iteration step count in extrapolation

  chainprofile() {stepcount=NULL; reset_tp();}  ///< initialization 

  void initstep(const int n) {
    if (!stepcount) {
      stepcount=new int[n];
      for (int i=0; i<n; i++) stepcount[i]=0;
    }
    itermax = n;
  }

  chainprofile& operator = (const chainprofile& _c) {
      if (stepcount) delete[] stepcount;
      itermax  = _c.itermax;
      t_apw    = _c.t_apw   ; 
      t_uplink = _c.t_uplink; 
      t_lf     = _c.t_lf    ; 
      t_ep     = _c.t_ep    ; 
      t_pext   = _c.t_pext  ; 
      t_dense  = _c.t_dense ; 
      t_init   = _c.t_init  ; 
      t_newdt  = _c.t_newdt ; 
      t_check  = _c.t_check ; 
      t_sym    = _c.t_sym   ; 
      itercount= _c.itercount;
      stepcount=new int[itermax];
      for (int i=0; i<itermax; i++) stepcount[i] = _c.stepcount[i];
      return *this;
  }

  /*! reset the time*/
  void reset_tp(){
    itermax=0;
    t_apw=0.0;
    t_uplink=0.0;
    t_lf=0.0;
    t_ep=0.0;
    t_pext=0.0;
    t_dense=0.0;
    t_init=0.0;
    t_newdt=0.0;
    t_check=0.0;
    t_sym=0.0;
    if(stepcount) delete [] stepcount;
    stepcount=NULL;
    itercount=0;
  }

  ~chainprofile() { if (stepcount) delete[] stepcount;}

  //! print profile
  /*!
    @param[in] fout: out stream
    @param[in] i: step index
   */
  void print(std::ostream & fout, const int i) {
      fout<<"Time_profile: Step: "<<i
          <<"  Acc+Pot(s): "<<t_apw
          <<"  Update_link(s): "<<t_uplink
          <<"  Leap-frog(s): "<<t_lf
          <<"  Extrapolation(s): "<<t_ep
          <<"  Symplectic(s): "<<t_sym
          <<"  Perturbation(s): "<<t_pext
          <<"  Dense_output(s): "<<t_dense
          <<"  New_ds_(s): "<<t_newdt
          <<"  check(s): " <<t_check
          <<std::endl;
  }

};
    
#endif

//declaration
template <class particle_> class chain;
template <class particle_> class chainlist;
class chainpars;
class chaininfo;
class chainslowdown;

//! external acceleration function pointer
/*!
  Use to predict perturber at new time t
  @param[out] acc: acceleration to p from perturbers (should be reset in this function)
  @param[in] t: time interval for prediction
  @param[in] p: particle in chain
  @param[in] np: number of particles
  @param[in] pert: perturber particle
  @param[in] force: perturber acceleration class used for prediction
  @param[in] npert: number of perturbers
  @param[in] pars: extra parameters 
  \return user-defined status (defaulted should be zero)
 */
template<class particle_, class pertparticle_, class force_, class extpar_>
using ext_Acc = int (*)(Float3 *acc, const Float t, particle_ *p, const int np, pertparticle_ *pert, force_ *fpert, const int npert, extpar_ *pars);

//! Function pointer type to function for calculation of acceleration (potential) and the component of time transformation function \f$\partial W_{ij}/\partial \mathbf{x}_i\f$ and \f$W_{ij}\f$ (from particle j to particle i).
/*!     
  Notice the particle i and j are also passed as arguments. But it is only used to get extra information in addition to relative position xij. The velocity information should not be used. Otherwise the Leap-frog algorithm is broken.
  @param[out] Aij: acceleration vector for particle i.
  @param[out] Pij: potential of particle i from particle j (cumulative total potential only count the case of j>i)
  @param[out] dWij: time transformation function partial derivate \f$\partial W_{ij}/\partial \mathbf{x}_i\f$ (component from j to i; used when #beta>0; see ARC::chainpars.setabg()) .
  @param[out] Wij: time transformation function component from j to i (used when #beta>0; notice in \ref ARC::chain the cumulative total W only count the case of j>i).
  @param[in] Xij: relative position (1:3) \f$ \mathbf{x}_j - \mathbf{x}_i \f$
  @param[in] pi: particle i.
  @param[in] pj: particle j.
  @param[in] pars: User-defined interaction parameter class type data
  \return user-defined status (defaulted should be zero)
*/
template<class particle_, class extpar_>
using pair_AW = int (*) (Float* Aij, Float &Pij, Float *dWij, Float &Wij, const Float* Xij, const particle_ &pi, const particle_ &pj, extpar_* pars);

////! Function pointer type to function for calculation of acceleration (potential) from particle j to particle i.
///*!
//  Notice the positions of particles are given individually. Please don't use position and velocity of particle pi, pj since the reference frame may be wrong.
//  @param[out]  Aij: acceleration vector of particle i.
//  @param[out]  Pij: potential of particle i from j
//  @param[in]  xi: position vector i.
//  @param[in]  xj: position vector j.
//  @param[in]  pi: particle i.
//  @param[in]  pj: particle j.
//  @param[in]  pars: User-defined interaction parameter class type data
//*/
//template<class particle_, class extpar_>
//using pair_Ap = void (*) (Float * Aij, Float &Pij, const Float* xi, const Float* xj, const particle_ &pi, const particle_ &pj, extpar_* pars);

//! Function pointer type to function for calculation the timescale of two-body motion
/*!
  @param[in] m1: mass of particle 1
  @param[in] m2: mass of particle 2
  @param[in] X: relative position vector
  @param[in] V: relative velocity vector
  @param[in] pars: User-defined interaction parameter class type data
*/
template<class particle_, class extpar_>
using pair_T = Float (*) (const Float m1, const Float m2, const Float* x, const Float* v, extpar_* pars);


//! The chain parameter controller class
/*!
  This class control acceleration function, integration methods and error parameters for \ref chain
 */
class chainpars{
template <class T> friend class chain;
private:
  // time step integration parameter
  Float alpha; ///< logH cofficient
  Float beta;  ///< TTL cofficient
  Float gamma; ///< constant

  // extrapolation control parameter
  int exp_method;         ///< 1: Polynomial method; others: Rational interpolation method
  int exp_sequence;       ///< 1: Romberg sequence {h, h/2, h/4, h/8 ...}; 2: Bulirsch & Stoer (BS) sequence {h, h/2, h/3, h/4, h/6, h/8 ...}; 3: 4k sequence {h, h/2, h/6, h/10, h/14 ...}; 4: Harmonic sequence {h, h/2, h/3, h/4 ...} (defaulted 2. BS sequence)
  int exp_itermax; ///< maximum times for iteration.
  int den_intpmax; ///< maximum derivates for dense output interpolation

  int* step; ///< substep sequence
  int** bin_index; ///< binomial coefficients

  int sym_k; ///< symplectic cofficients array size
  int sym_n; ///< symplectic integrator order
  Float sym_inv_n; ///< inverse order
  Float sym_An; ///< 0.5^order
  Float2* sym_coff; ///< cofficients for symplectic integrator (c_k, d_k)
  SYM::symcumck* sym_order;   ///< index for increasing order of cumsum c_k (index, cumsum(c_k))

  int auto_step;           ///< if 0: no auto step; if 1: use extrapolation error to estimate next step modification factor; if 2: use min X/(g V) and V/(g A) to obtain next step; if 3: use maximum sequence index to control next step; if 4: use mimimum kepler period of every two neigbor members to estimate the next step modification factor.
  Float auto_step_fac_min; ///< minimum reduction factor
  Float auto_step_fac_max; ///< maximum reduction factor
  Float auto_step_eps;     ///< coefficient
  int auto_step_iter_min;   ///< mimimum iteration level
  int auto_step_iter_max;   ///< maximum iteration level

  // pair force
  // pair_AW pp_AW;  ///< acceleration and dW/dr of two particle
  // pair_Ap pp_Ap;  ///< accelaration of two particle
  // pair_T  pp_T;   ///< two-body timescale calculator
  // ext_Acc ext_A;  ///< external acceleration function, if give, external force will be added to pert force
  void* pp_AW;
  void* pp_T;
  void* ext_A;
  std::type_index* pp_AW_type;
  std::type_index* pp_T_type;
  std::type_index* ext_A_type;
    
public:
  
  // time step
  Float dtmin; ///< minimum physical time step
  Float dterr; ///< physical time error criterion

  // extrapolation control parameter
  Float exp_error;        ///< relative error requirement for extrapolation
  bool exp_fix_iter;       ///< flag showing whether the times of iteration is fixed or not

  //! constructor with defaulted parameters
  /*! - ARC method use logarithmic Hamiltonian (logH) (#alpha = 1.0, #beta = 0.0 #gamma = 0.0).
      - Phase/energy error limit #exp_error = 1e-10.
      - Minimum physical time step #dtmin = 5.4e-20.
      - Time synchronization (relative to time step) physical time error limit #dterr = 1e-6.
      - extrapolation sequence is not set
      - high-order Symplectic integrator is switched off
      - No auto-step
   */
  chainpars() {
    pp_AW = ext_A = pp_T = NULL;
    pp_AW_type = ext_A_type = pp_T_type = NULL;
    step = NULL;
    bin_index = NULL;
    sym_coff = NULL;
    sym_order= NULL;
    setabg();
    setErr();
    // For extrapolation integration
    setIterSeq();
    setIntp();
    setIterConst();
    setSymOrder(0);
    setAutoStep(0);
  }

//  //! constructor
//  /*!
//    All parameters can be set except anto-step method (defaulted is zero)
//    @param [in] a,b,g: ARC time transformation method coefficients (\f$ dt = ds/[a *(logH) + b * (TTL) + g])\f$. \n
//                - a: Logarithmic Hamiltonian (logH) method coefficient #alpha (0.0, 1.0)
//                - b: Time-Transformed Leapfrog (TTL) method coefficient #beta (0.0, 1.0)
//                - g: Constant coefficient (no time transformation) #gamma
//    @param [in] error: Phase/energy error limit (defaulted #exp_error = 1e-10)
//    @param [in] dtm: Minimum physical time step (defaulted #dtmin = 5.4e-20)
//    @param [in] dte: Relative time synchronization error limit (defaulted #dterr = 1e-6)
//    @param [in] itermax: Maximum extrapolation sequence index (accuracy order/iteration times) (defaulted #exp_itermax = 20)
//    @param [in] ext_method: 1: Polynomial interpolation method; others: Rational interpolation method (defaulted: Rational)
//    @param [in] ext_sequence: 1: Romberg sequence {h, h/2, h/4, h/8 ...}; 2: Bulirsch & Stoer (BS) sequence {h, h/2, h/3, h/4, h/6, h/8 ...}; 3: 4k sequence {h, h/2, h/6, h/10, h/14 ...}; others: Harmonic sequence {h, h/2, h/3, h/4 ...} (defaulted 2. BS sequence)
//    @param [in] dense_intpmax: maximum derivate index for dense output interpolation (defaulted #itermax/2)
//    @param [in] ext_iteration_const: true: the maximum sequence index (iteration times) is fixed to itermax; false: adjust the maximum sequence index by error criterion (false)
//   */
//  chainpars(const Float a, const Float b, const Float g, const Float error=1E-10, const Float dtm=5.4e-20, const Float dte=1e-6, const int itermax=20, const int ext_method=2, const int ext_sequence=2, const int dense_intpmax=10, const bool ext_iteration_const=false) {
//    pp_AW = ext_A = pp_T = NULL;
//    pp_AW_type = ext_A_type = pp_T_type = NULL;
//    step = NULL;
//    bin_index = NULL;
//    setabg(a,b,g);
//    setErr(error,dtm,dte);
//    setIterSeq(itermax,ext_sequence,dense_intpmax);
//    setIntp(ext_method);
//    setIterConst(ext_iteration_const);
//    setAutoStep(0);
//  }

  //! destructor
  ~chainpars() {
    if (bin_index!=NULL) {
      if (step!=NULL) {
        for (int i=0; i<step[exp_itermax]; i++)
          if (bin_index[i]!=NULL) delete[] bin_index[i];
      }
      delete[] bin_index;
    }
    if (step!=NULL) delete[] step;
    if (pp_AW_type!=NULL) {
        delete pp_AW_type;
    }
    if (ext_A_type!=NULL) {
        delete ext_A_type;
    }
    if (pp_T_type!=NULL) {
        delete pp_T_type;
    }
    if (sym_coff!=NULL) delete[] sym_coff;
    if (sym_order!=NULL) delete[] sym_order;
  }

  //! Set acceleration, potential, time transformation function \f$\partial W/\partial r\f$ and \f$W\f$ calculator
  /*! Set acceleration, potential, time transformation function \f$\partial W/\partial r\f$ and \f$W\f$ for two particles. This is the basic functions used for interaction between chain members and from perturbers.
    @param [in] aw: acceleration, potential and time transformation function \f$\partial W/\partial r\f$, \f$W\f$ of two particles calculation function pointer with pair_AW type. (interaction between memebrs). Notice this function defines \f$\partial W/\partial r\f$ and \f$W\f$, and these time transformation functions are only used when #beta>0 (see setabg()).
    @param [in] exta: external force(acceleration) function pointer with ext_Acc type
    @param [in] at: two-body timescale calculation function pointer with pair_T type. (this will be used for new step size estimation)
  */
  template<class particle_, class pertparticle_, class pertforce_, class extpar_>
  void setA(pair_AW<particle_,extpar_> aw, ext_Acc<particle_, pertparticle_, pertforce_, extpar_> exta=NULL, pair_T<particle_, extpar_> at=NULL) {
    pp_AW = (void*)aw;
    ext_A = (void*)exta;
    pp_T  = (void*)at;
    pp_AW_type = new std::type_index(typeid(aw));
    ext_A_type = new std::type_index(typeid(exta));
    pp_T_type  = new std::type_index(typeid(at));
    // safety check
    if (pp_AW==NULL) {
      std::cerr<<"Error: accelaration and time trasnformation calculator function is NULL\n";
      abort();
    }
  }
  
     
  //! Set time step transformation parameter
  /*!
      Set parameter a,b,g where physical time step \f$ dt = ds/[a *(logH) + b * (TTL) + g]\f$ \n
      @param [in] a: Logarithmic Hamiltonian (logH) method coefficient #alpha (0.0, 1.0)
      @param [in] b: Time-Transformed Leapfrog (TTL) method coefficient #beta (0.0, 1.0)
      @param [in] g: Constant coefficient (no time transformation) #gamma
  */
  void setabg(const Float a=1.0, const Float b=0.0, const Float g=0.0) {
    alpha = a;
    beta = b;
    gamma = g;
    // safety check
    if (alpha==0&&beta==0&&gamma==0) {
      std::cerr<<"Error: alpha, beta and gamma cannot be all zero!\n";
      abort();
    }
  }

  //! Get time step transformation parameter
  /*!
      Get parameter a,b,g where physical time step \f$ dt = ds/[a *(logH) + b * (TTL) + g]\f$ \n
      @param [in] a: Logarithmic Hamiltonian (logH) method coefficient #alpha (0.0, 1.0)
      @param [in] b: Time-Transformed Leapfrog (TTL) method coefficient #beta (0.0, 1.0)
      @param [in] g: Constant coefficient (no time transformation) #gamma
  */
  void getabg(Float &a, Float &b, Float &g) const {
    a = alpha;
    b = beta;
    g = gamma;
  }

  //! Set Symplectic integrator order for Symplectic_step_forwards
  /*!
    @param [in] n: symplectic integrator order, should be even, otherwise reduce to closest even number; if 0, not set; if negative, use second solution from Yoshida (1990), but only -6 and -8 works
   */
  void setSymOrder(const int n) {
      sym_n = n<0?-n:n;
      if(n!=0) sym_inv_n = 1.0/(Float)sym_n;
      sym_An = pow(Float(0.5),sym_n);
      int k = n/2;
      if (n>0) sym_k = std::pow(3,k-1)+1;
      else if(n==-6) sym_k = 8;
      else if(n==-8) sym_k = 16;
      else sym_k = 0;

      if(sym_k>0) {
          if (sym_coff!=NULL) delete[] sym_coff;
          sym_coff = new Float2[sym_k];
          if (sym_order!=NULL) delete[] sym_order;
          sym_order = new SYM::symcumck[sym_k];
          SYM::symplectic_cofficients(sym_coff, sym_order, k, sym_k);
      }
      sym_n = n;
  }

  //! Set error paremeters
  /*!
    @param [in] error: phase/energy relative error requirement for extrapolation (defaulted #exp_error = 1e-10)
    @param [in] dtm: minimum physical time step allown (defaulted #dtmin = 5.4e-20)
    @param [in] dte: Time synchronization error limit (defaulted #dterr = 1e-6)
   */
  void setErr(const Float error=1E-10, const Float dtm=5.4e-20, const Float dte=1e-6) {
    if(dte>0) dterr = dte;
    if(dtm>0) dtmin = dtm;
    if(error>0) exp_error = error;
  }

  //! Set interpolation method
  /*! Set interpolation method for extrapolation integration
    @param [in] method: 1: Polynomial method; others: Rational interpolation method (defaulted Rational)
   */
  void setIntp(const int method=2)  {
    exp_method = method;
  }

  //! Get interpolation method index
  /*! 
    \return method index: 1: Polynomial method; others: Rational interpolation method (defaulted Rational)
  */
  int getIntp() const {
    return exp_method;
  }

  //! Set whether to use constant iteration number in extrapolation integration
  /*!
    @param [in] ext_iteration_const: true: the maximum sequence index (iteration times) is fixed to itermax; false: adjust the maximum sequence index by error criterion (false)
  */
  void setIterConst (const bool ext_iteration_const=false) {
    exp_fix_iter = ext_iteration_const;
  }
  
  //! Set extrapolation maximum iteration number and sequences
  /*! 
    @param [in] itermax: Maximum extrapolation sequence index (accuracy order/iteration times) (defaulted #exp_itermax = 20)
    @param [in] sequence: 1: Romberg sequence {h, h/2, h/4, h/8 ...}; 2: Bulirsch & Stoer (BS) sequence {h, h/2, h/3, h/4, h/6, h/8 ...}; 3: 4k sequence {h, h/2, h/6, h/10, h/14 ...}; others: Harmonic sequence {h, h/2, h/3, h/4 ...} (defaulted 0. no sequence)
    @param [in] intpmax: maximum derivate index for dense ouput interpolation (defaulted #itermax/2)
  */
  void setIterSeq(const int itermax=20, const int sequence=0, const int intpmax=0) {
    if(itermax>0) exp_itermax = itermax;
    if(sequence>=0) exp_sequence = sequence;

    // delete binomial array
    if (bin_index!=NULL) {
      if (step!=NULL) {
        for (int i=0; i<step[exp_itermax]; i++)
          if (bin_index[i]!=NULL) delete[] bin_index[i];
      }
      delete[] bin_index;
    }
    
    // reset step array
    if (step!=NULL) delete[] step;

    if(exp_sequence>0) {
        step = new int[itermax+1];

        // calculate sequences of steps
        // Romberg (even) sequence {h, h/2, h/4, h/8 ...}
        if (sequence==1) EP::seq_Romberg(step,itermax+1);
        // Bulirsch & Stoer sequence {h, h/2, h/3, h/4, h/6, h/8 ...}
        else if (sequence==2) EP::seq_BS(step,itermax+1);
        // E. Hairer (4k) sequences {h, h/2, h/6, h/10, h/14 ...}
        else if (sequence==3) EP::seq_Hairer(step,itermax+1);
        // Harmonic sequences {h, h/2, h/3, h/4 ...}
        else if (sequence==4) EP::seq_Harmonic(step,itermax+1);
        else {
            std::cerr<<"Error: Unknown sequence index "<<sequence<<std::endl;
            abort();
        }

        // calculate binomial coefficients
        bin_index = new int*[step[itermax]];
        for (int i=0; i<step[itermax]; i++) {
            bin_index[i] = new int[i+1];
            if (i>0) EP::binomial_recursive_generator(bin_index[i],bin_index[i-1],i+1);
            else EP::binomial_recursive_generator(bin_index[i],NULL,i+1);
        }
    
        if (intpmax>itermax) {
            std::cerr<<"Error: dense output interpolation derivate index ("<<intpmax<<") cannot be larger than extrapolation maximum sequence index ("<<itermax<<")!\n";
            abort();
        }
        else if(intpmax==0) den_intpmax=itermax/2;
        else if(intpmax>0) den_intpmax = intpmax;

    }    
  }

  //! set maximum derivate index for dense output interpolation
  /*!
    @param [in] intpmax: maximum derivate index for dense ouput interpolation (defaulted #itermax/2)
   */
  void setDenIntpmax(const int intpmax) {
    if (intpmax>exp_itermax) {
      std::cerr<<"Error: dense output interpolation derivate index ("<<intpmax<<") cannot be larger than extrapolation maximum sequence index ("<<exp_itermax<<")!\n";
      abort();
    }
    den_intpmax = intpmax;
  }

  //! Get maximum derivate index for dense output interpolation
  /*!
    \return  maximum derivate index for dense ouput interpolation (defaulted #itermax/2)
   */
  int getDenIntpmax() const {
    return den_intpmax;
  }

  //! Get sequence method indices
  /*  
    \return sequence method index: 1: Romberg sequence {h, h/2, h/4, h/8 ...}; 2: Bulirsch & Stoer (BS) sequence {h, h/2, h/3, h/4, h/6, h/8 ...}; 3: 4k sequence {h, h/2, h/6, h/10, h/14 ...}; others: Harmonic sequence {h, h/2, h/3, h/4 ...} (defaulted 2. BS sequence)
  */  
  int getSeq() const {
    return exp_sequence;
  }

  //! Get extrapolation maximum iteration times (maximum sequence index)
  /*  
    \return maximum iteration times
  */  
  int getIter() const {
    return exp_itermax;
  }

  //! Determine auto-step parameter
  /*! @param[in] option: auto-step method
    - 0. no auto-step
    - 1. use extrapolation error to estimate next step
    - 2. use min(\f$X/(gV)\f$,\f$V/(gA)\f$) to estimate next step
    - 3: use maximum sequence index to control next step. If index >\a iter_max, modification factor = \a factor_min; if index < \a iter_min, modification factor = \a factor_max;
    - 4: use mimimum user-defined two-body timescale of every two neigbor members to estimate next step
      @param[in] factor_min: if \a option = 1->3, it is the minimum step reduction factor (defaulted: 0.7)
      @param[in] factor_max: if \a option = 1->3, it is the maximum step reduction factor (defaulted: 1.3)
      @param[in] eps: coefficient. If \a option = 2 or 4, coefficient is the multiplying factor for the estimated next step (defaulted: 0.125)
      @param[in] iter_min: if \a option = 3, it is the minimum sequence index (iteration times) criterion during extrapolation intergration (defaulted: 5)
      @param[in] iter_max: if \a option = 3, it is the maximum sequence index (iteration times) criterion during extrapolation intergration (defaulted: 17)
  */
  void setAutoStep(const int option, const Float factor_min=0.7, const Float factor_max=1.3, const Float eps=0.5, const int iter_min=5, const int iter_max=17) {
    if (option<0||option>4) {
      std::cerr<<"Error: autostep options should be set from 0 to 4, current value: "<<option<<"!\n";
      abort();
    }
    auto_step = option;
    if (factor_min>1.0||factor_min<0.0) {
      std::cerr<<"Error: step reduction factor is limited to the range [0,1.0], input value is "<<factor_min<<"!\n";
      abort();
    }
    auto_step_fac_min = factor_min;
    if (factor_max<1.0) {
      std::cerr<<"Error: step increasing factor is limited to the range (>1.0), input value is "<<factor_max<<"!\n";
      abort();
    }
    auto_step_fac_max = factor_max;
    if (eps<0.0) {
      std::cerr<<"Error: multiplying coefficient for next estimated step should be positive, input value is "<<eps<<"!\n";
      abort();
    }
    auto_step_eps=eps;
    if (iter_min>exp_itermax) {
      std::cerr<<"Error: minimum iteration times criterion should be less than extrapolation maximum iteration times ("<<exp_itermax<<"), input value is "<<iter_min<<"!\n";
      abort();
    }
    auto_step_iter_min=iter_min;
    if (iter_max>exp_itermax) {
      std::cerr<<"Error: maximum iteration times criterion should be less than extrapolation maximum iteration times ("<<exp_itermax<<"), input value is "<<iter_max<<"!\n";
      abort();
    }
    auto_step_iter_max=iter_max;
  }

  //! Get auto-step parameters
  /*! @param[in] option: auto-step method
    - 0. no auto-step
    - 1. use extrapolation error to estimate next step
    - 2. use min(\f$X/(gV)\f$,\f$V/(gA)\f$) to estimate next step
    - 3: use maximum sequence index to control next step. If index >\a iter_max, modification factor = \a factor_min; if index < \a iter_min, modification factor = \a factor_max;
    - 4: use mimimum user-defined two-body timescale of every two neigbor members to estimate next step
      @param[in] factor_min: if \a option = 1->3, it is the minimum step reduction factor (defaulted: 0.7)
      @param[in] factor_max: if \a option = 1->3, it is the maximum step reduction factor (defaulted: 1.3)
      @param[in] eps: coefficient. If \a option = 2 or 4, coefficient is the multiplying factor for the estimated next step (defaulted: 0.125)
      @param[in] iter_min: if \a option = 3, it is the minimum sequence index (iteration times) criterion during extrapolation intergration (defaulted: 5)
      @param[in] iter_max: if \a option = 3, it is the maximum sequence index (iteration times) criterion during extrapolation intergration (defaulted: 17)
   */
  void getAutoStep(int &option, Float &factor_min, Float &factor_max, Float &eps, int &iter_min, int &iter_max) const {
    option = auto_step;
    factor_min = auto_step_fac_min;
    factor_max = auto_step_fac_max;
    eps = auto_step_eps;
    iter_min = auto_step_iter_min;
    iter_max = auto_step_iter_max;
  }

  //! parameters dumping
  /*! Dump parameters into file. Notice dynamic array data will not be dumped.
    The dumping data includes: #alpha, #beta, #gamma, #exp_method, #exp_sequence, #exp_itermax, #exp_error, #exp_fix_iter, #dtmin, #dterr, #auto_step, auto_step parameters
    @param[in] pout: std::FILE pointer to dump data
   */
    void dump(std::FILE *pout) const {
      fwrite(&alpha, sizeof(Float),1,pout);
      fwrite(&beta,  sizeof(Float),1,pout);
      fwrite(&gamma, sizeof(Float),1,pout);
      fwrite(&exp_method,  sizeof(int),1,pout);
      fwrite(&exp_sequence,sizeof(int),1,pout);
      fwrite(&exp_itermax, sizeof(int),1,pout);
      fwrite(&den_intpmax, sizeof(int),1,pout);
      fwrite(&exp_error,   sizeof(Float),1,pout);
      fwrite(&exp_fix_iter,sizeof(bool),1,pout);
      fwrite(&dtmin, sizeof(Float),1,pout);
      fwrite(&dterr, sizeof(Float),1,pout);
      fwrite(&sym_n, sizeof(int),1,pout);
      fwrite(&auto_step, sizeof(int),1,pout);
      fwrite(&auto_step_fac_min, sizeof(Float),1,pout);
      fwrite(&auto_step_fac_max, sizeof(Float),1,pout);
      fwrite(&auto_step_eps,     sizeof(Float),1,pout);
      fwrite(&auto_step_iter_min,sizeof(int),1,pout);
      fwrite(&auto_step_iter_max,sizeof(int),1,pout);
  }

  //! parameters loading from a dumped file
  /*! Load parameters from a dumped file. All dynamical array will be initialized after reading parameters.
    Notice this function will load all parameters except acceleration function pointers #pp_AW and #pp_Ap. 
    setA() should be called before chain integration.
    The reading data list is shown in dump()
    @param [in] filename: std::FILE pointer for reading the data
   */
  void read(std::FILE *pin) {
      step = NULL;
      bin_index = NULL;
      int rn;
      Float a,b,g;
      rn = fread(&a, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read alpha!\n";
        abort();
      }
      rn = fread(&b, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read beta!\n";
        abort();
      }
      rn = fread(&g, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read gamma!\n";
        abort();
      }
      setabg(a,b,g);

      Float error,dtm,dte;
      int itermax,intpmax;
      int ext_method, ext_sequence;
      bool ext_iteration_const;
      
      rn = fread(&ext_method, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read exp_method!\n";
        abort();
      }
      rn = fread(&ext_sequence, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read exp_sequence!\n";
        abort();
      }
      rn = fread(&itermax, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read exp_itermax!\n";
        abort();
      }
      rn = fread(&intpmax, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read den_intpmax!\n";
        abort();
      }
      rn = fread(&error, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read exp_error!\n";
        abort();
      }
      rn = fread(&ext_iteration_const, sizeof(bool), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read exp_fix_iter!\n";
        abort();
      }
      rn = fread(&dtm, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read dtmin!\n";
        abort();
      }
      rn = fread(&dte, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read dterr!\n";
        abort();
      }

      int symn;
      rn = fread(&symn, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read dterr!\n";
        abort();
      }
      setSymOrder(symn);
      setErr(error,dtm,dte);
      setIterSeq(itermax,ext_sequence,intpmax);
      setIntp(ext_method);
      setIterConst(ext_iteration_const);

      int as;
      Float asmin, asmax, aseps;
      int asitmin, asitmax;
      rn = fread(&as, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read auto_step!\n";
        abort();
      }
      rn = fread(&asmin, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read auto_step_fac_min!\n";
        abort();
      }
      rn = fread(&asmax, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read auto_step_fac_max!\n";
        abort();
      }
      rn = fread(&aseps, sizeof(Float), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read auto_step_eps!\n";
        abort();
      }
      rn = fread(&asitmin, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read auto_step_iter_min!\n";
        abort();
      }
      rn = fread(&asitmax, sizeof(int), 1 ,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read auto_step_iter_max!\n";
        abort();
      }
      setAutoStep(as,asmin,asmax,aseps,asitmin,asitmax);
  }

  //! Print parameter table
  /*! Print parameter table
    @param[in] fout: ofstream for printing
   */
  void print(std::ostream & fout) {
    fout<<"======================Chain parameter table=========================\n"
        <<"Time step transformation parameters:\n"
        <<"  alpha = "<<alpha<<"  beta = "<<beta<<"  gamma = "<<gamma<<std::endl;
    if (exp_sequence>0) {
        fout<<"Extrapolation parameters:\n"
            <<"  Interpolation method:    "<<((exp_method==1)?"Polynomial":"Rational")<<std::endl
            <<"  Sequence:                ";
        if (exp_sequence==1) fout<<"Romberg sequence {h, h/2, h/4, h/8 ...}\n";
        else if(exp_sequence==2) fout<<"Bulirsch & Stoer sequence {h, h/2, h/3, h/4, h/6, h/8 ...}\n";
        else if(exp_sequence==3) fout<<"4k sequence {h/2, h/6, h/10, h/14 ...}\n";
        else if(exp_sequence==4) fout<<"Harmonic sequence {h, h/2, h/3, h/4 ...}\n";
        else fout<<"Not available\n";
        fout<<"  Maximum iteration times: "<<exp_itermax<<std::endl
            <<"  Maximum dense output derivate index: "<<den_intpmax<<std::endl
            <<"Auto-step parameters:\n"
            <<"  Auto-step method: ";
        if (auto_step==0) fout<<"None\n";
        else if(auto_step==1) fout<<"Use extrapolation error\n"
                                  <<"  Minimum reduction factor: "<<auto_step_fac_min<<std::endl
                                  <<"  Maximum increasing factor: "<<auto_step_fac_max<<std::endl;
        else if(auto_step==2) fout<<"Use min X/(g V) and V/(g A)\n"
                                  <<"  Minimum reduction factor: "<<auto_step_fac_min<<std::endl
                                  <<"  Maximum increasing factor: "<<auto_step_fac_max<<std::endl
                                  <<"  Multiplied coefficient:   "<<auto_step_eps<<std::endl;
        else if(auto_step==3) fout<<"Use maximum sequence index to control next step\n"
                                  <<"  Reduction factor: "<<auto_step_fac_min<<std::endl
                                  <<"  Increasing factor: "<<auto_step_fac_max<<std::endl
                                  <<"  Mimimum iteraction level: "<<auto_step_iter_min<<std::endl
                                  <<"  Maximum iteraction level: "<<auto_step_iter_max<<std::endl;
        else if(auto_step==4) fout<<"Use minimum user-defined two-body timescale of each neigbor pairs\n"
                                  <<"  Multiplied coefficient:   "<<auto_step_eps<<std::endl;
        fout<<"Iteration times fixed?: "<<exp_fix_iter<<std::endl;
    }
    if (sym_n!=0) {
        fout<<"Symplectic integration paramters:\n"
            <<"  Integrator order: "<<std::abs(sym_n)<<std::endl
            <<"  Step pair (DK) number: "<<sym_k<<std::endl;
    }
    fout<<"  Phase/energy error criterion:    "<<exp_error<<std::endl
        <<"  Time sychronization error limit: "<<dterr<<std::endl;

    fout<<"  Minimum physical time:           "<<dtmin<<std::endl;
    fout<<"====================================================================\n";
  }
};

//! class for storing the information of chain (for decision making)
/*!
    It is used to store the information when an accident happen with chain.status != 0.
 */
class chaininfo{
public:
  int  status;    ///< current status of integration
  // extrapolation case
  int  intcount;  ///< current extrapolation sequence index when an accident happen
  int  inti;      ///< current number of substeps
  Float ds,subds,subdt;      ///< step size, substep size, substep corresponding time step
  Float perr,perr0;  ///< phase error (current and previous)
  Float eerr,eerr0;  ///< energy error (current and previous)
  // symplectic case
  Float ds1,ds2;   ///< step buffer
  int stepcount;    ///< stepcount
  
  int  i1, i2;    ///< index of particles causing the accident
  int  num;       ///< number of particles
  Float toff;    ///< physical ending time
  Float Ekin,Pot,W;  ///< kinetic energy, potential energy and time transformation function
  Float terr;    ///< time error
  Float* data;   ///< chain data for backuping

  //! constructor
  /*!
    Set all variables to infinity
    @param[in] n: number of particles
   */
  chaininfo(const int n) {
    status = 0;
    const Float infd = std::numeric_limits<Float>::infinity();
    intcount = inti = i1 = i2 = -1;
    ds = subds = subdt = toff = perr = perr0 = eerr = eerr0 = terr = Ekin = Pot = W = infd;
    const int dsize=6*n-3;
    num = n;
    data=new Float[dsize+1];
    data[dsize]=dsize;
  }

  //! reset function
  /*!
    Notice the data will not be deleted.
   */
  void clear() {
    status = 0;
    const Float infd = std::numeric_limits<Float>::infinity();
    intcount = inti = i1 = i2 = -1;
    ds = subds = subdt = toff = perr = perr0 = eerr = eerr0 = terr = Ekin = Pot = W = infd;
  }
  
  //! destructor
  ~chaininfo() {
    if (data) delete[] data;
  }

  //! print chain information
  /*! Print chain information
      @param[in] fout: ofstream for printing
      @param[in] precision: printed precision for one variable
  */
  void print(std::ostream & fout, const int precision=10) {
    Float inf=std::numeric_limits<Float>::infinity();
    fout<<"=======================Chain information============================\n";
    fout<<std::setprecision(precision);
    if (ds    <inf) fout<<"step size: "<<ds<<std::endl;
    if (subds <inf) fout<<"sub-step size: "<<subds<<std::endl;
    if (subdt <inf) fout<<"sub-step physical time step size: "<<subdt<<std::endl;
    if (toff  <inf) fout<<"Physical ending time: "<<toff<<std::endl;
    if (terr  <inf) fout<<"Time error: "<<terr<<std::endl;
    if (intcount>0) fout<<"stored sequence index (if extrapolation is used): "<<intcount<<std::endl;
    if (inti    >0) fout<<"stored integration sub step index: "<<inti<<std::endl;
    if (i1      >0) fout<<"particle indices that cause the accident: "<<i1<<" "<<i2<<std::endl;
    if (Ekin  <inf) fout<<"Kinetic energy: "<<Ekin<<std::endl;
    if (Pot   <inf) fout<<"Potential energy: "<<Pot<<std::endl;
    if (W     <inf) fout<<"Time transformation function W: "<<W<<std::endl;
    if (eerr  <inf) fout<<"Current energy error: "<<eerr<<"; previous: "<<eerr0<<std::endl;
    if (perr  <inf) fout<<"Current phase error: "<<perr<<"; previous: "<<perr0<<std::endl;
    fout<<"Physical time: "<<data[0]<<std::endl
        <<"Time momemtum Pt: "<<data[1]<<std::endl
        <<"Integrated time transformation function w: "<<data[2]<<std::endl
        <<"Relative positions and velocity: \n";
    for (int i=1; i<num; i++) {
      fout<<"No. "<<i<<" ";
      for (int k=0; k<3; k++) fout<<data[3*i+k]<<" ";
      for (int k=0; k<3; k++) fout<<data[3*(i+num-1)+k]<<" ";
      fout<<std::endl;
    }
    fout<<"====================================================================\n";
  }

  //! status message
  /*!
    Output the status message, for status 1 to 5, the error messages are output. The status number includes the return value of user-defined acceleration functions.
    @param[in] fout: ofstream for printing
    @param[in] precision: printed precision for one variable
    @param[in] stat: user-defined status number (non-zero value)
    @param[in] message: user-defined status corresponding output message), this will overlap defaulted message when \a stat=1 to 5
   */
  void ErrMessage(std::ostream & fout, const int precision=10, const int stat=0, const char* message=NULL) {
    if (stat!=0&&status==stat) fout<<"Status ["<<stat<<"]: "<<message<<std::endl;
    else {
      if (status==0) fout<<"Normal status [0]:\n";
      else if (status==1) fout<<"Error [1]: extrapolation cannot converge anymore, but energy error is larger than 100 * criterion!\n";
      else if (status==2) fout<<"Error [2]: maximum iteration step number reached, but energy error is larger than 100 * criterion!\n";
      else if (status==3) fout<<"Error [3]: find root fails after 100 iterations; can not find ds to reach physical time toff!\n";
      else if (status==4) fout<<"Error [4]: physical time step too small!\n";
      else if (status==5) fout<<"Error [5]: Time interpolation is not monotonic!\n";
    }
    print(fout,precision);
  }
};
  
//! Slow-down parameter control class
/*! Determine the slow-down factor due to the perturbation and internal force
    \f$ \kappa = k_0 / [F_{pert,max}/F_{inner}] \f$
 */
class chainslowdown{
template <class T> friend class chain;
private:
    Float kappa;          // slow-down factor
    Float kappa_org;      // original slow-down factor
    Float kappa_max;      // maximum kappa factor
    Float f_ratio_sq_max;     // maximum perturbation force / inner force square recorded
    Float f_ratio_sq_peak;     // minimum perturbation force / inner force square recorded
    Float f_ratio_sq_last;    // last record of perturbation force / inner force square 
    Float df_ratio_sq_last;   // last record of difference of perturbation force / inner force square 
    Float t_real;         // current time
    Float t_record;       // last kappa update time
    Float kref;    ///< reference kappa factor; slow-down factor kappa = max(1,kref/(perturbation/internal force))
    Float period;        ///< period for checking
    //Float finnersq;     ///< inner force square reference for slow-down determination
    bool is_used;         ///< if false, slow-down is switched off

public:
    //! defaulted constructor
    chainslowdown() { reset(); }
    
    //! reset function
    void reset(){
        kappa = kappa_org = kappa_max = 1.0;
        f_ratio_sq_max = f_ratio_sq_peak = 0.0;
        f_ratio_sq_last = df_ratio_sq_last = 0.0;
        t_real = t_record = 0.0;
        kref = 1.0e-8;
        period = 0.0;
        is_used = false;
    }

    //! Update maximum perturbation force \f$ F_{pert,max}\f$
    /*! Update maximum perturbation force and record this input (only last one is recorded)
      @param[in] _fratiosq: new perturbation force square
     */
    void updateFRatioSqRange(const Float _f_ratio_sq) {
        // backup differential f_ratio_sq from last step
        Float df_ratio_sq_back= df_ratio_sq_last;
        // get new
        df_ratio_sq_last = _f_ratio_sq - f_ratio_sq_last;
        // update peak value when reach maximum peak
        if (df_ratio_sq_last<0 && df_ratio_sq_back>0) f_ratio_sq_peak = f_ratio_sq_last;
        // record f_ratio_sq
        f_ratio_sq_last  = _f_ratio_sq;
        // record the max of f_ratio_sq peak
        f_ratio_sq_max = f_ratio_sq_peak>f_ratio_sq_max? f_ratio_sq_peak: f_ratio_sq_max;
    }

    //! initial maximum perturbation force \f$ F_{pert,max}\f$
    /*! initial maximum perturbation force and record this input (only last one is recorded)
      @param[in] _fratiosq: new perturbation force square
     */
    void initialFRatioSqRange(const Float _f_ratio_sq) {
        df_ratio_sq_last = 1;
        f_ratio_sq_last  = _f_ratio_sq;
        f_ratio_sq_peak = f_ratio_sq_last;
        f_ratio_sq_max = f_ratio_sq_last;
    }

    //! initialize slow-down parameters
    /*! Set slow-down parameters, slow-down method will be switched on
      @param [in] period: Period of system 
      @param [in] k_ref: Fpert/Finner criterion (default 1.0e-5)
    */
    void setSlowDownPars(const Float _period, const Float _k_ref, const Float _kappa_max) {
        kappa = 1.0;
        kappa_org = 1.0;
        kappa_max = _kappa_max;
        period   = _period;
#ifdef ARC_DEBUG
        assert(period>0);
#endif
        t_record = t_real;
        kref     = _k_ref;
        is_used  = true;
    }

    //! Update real time
    /*! Update current real time of the system
        @param[in] dt_chain: chain inner integrated time with current slowdown factor, will be transferred and added to real time t_real.
     */
    void updateRealTime(const Float dt_chain) {
        t_real += dt_chain * kappa;
    }

    //! manually set kappa
    /* auto-adjust in the range (1.0, kappa_max)
     */
    void setKappa(const Float _kappa) {
        kappa = _kappa;
        kappa_org = _kappa;
        f_ratio_sq_max = kref;
        f_ratio_sq_last = kref;
        kappa = std::min(kappa, kappa_max);
        kappa = std::max(Float(1.0),kappa);
    }

    //! Update slow-down factor minimum
    /*! Update slow-down factor minimum
     */
    void updateKappaMin() {
        kappa = kref/sqrt(f_ratio_sq_max);
        kappa_org = kref/sqrt(f_ratio_sq_last);
        kappa = std::min(kappa, kappa_max);
        kappa = std::max(Float(1.0),kappa);
    }

    //! Update slow-down factor after one period using minimum record
    /*! Update slow-down factor after one period using minimum record
        @param[in] tend_real: ending physical time for integration, if tend-time<#period, \f$kappa = 1.0\f$
        @param[in] modify_factor: maximum modification factor for kappa (should be >1.0), if <0, initialization
     */
    void updateKappaMinPeriod(const Float modify_factor_limit=0.05) {
        if(is_used) {
#ifdef ARC_DEBUG
            assert(period>0);
#endif
            // update fratio max record after one orbital integration
            Float dt = t_real - t_record;
            if (dt>period) {
#ifdef ARC_DEBUG
                assert(f_ratio_sq_max>0.0);
#endif
                updateKappaMin();
                // limit kappa change due to modify_factor_limit
                if (dt>period*kappa) {
                    // update time record of modification
                    t_record = t_real;
                    // reset f ratio records
                    if (modify_factor_limit>0) {
                        if(f_ratio_sq_peak>f_ratio_sq_max) f_ratio_sq_max = std::min(f_ratio_sq_peak,f_ratio_sq_max*(1.0+modify_factor_limit));
                        else f_ratio_sq_max=std::max(f_ratio_sq_peak,f_ratio_sq_max/(1.0+modify_factor_limit));
                    }
                    else f_ratio_sq_max = f_ratio_sq_peak;
                }
            }
        }
    }

    //! adjust slow-down factor to reach same phase as real step dt
    /*! Adjust slow-down factor based on
        K * dt_sd - dt_sd = n * P (n is integer)
        n = (K - 1) * dt_sd /P = (K-1)/K * dt /P
        Thus first adjust n to integer as
        n(int) = int[(K-1)/K*dt/P]
        Then obtain new kappa:
        K_new = dt/(dt-n(int)*P)
      @param [in] dt: physical time step (assume dt is constant before next update of kappa) without slow-down factor
     */
    void adjustkappa(const Float dt) {
        if(is_used) {
            int kp = to_int((kappa-1.0)/kappa*dt/period);
            kappa = std::max(Float(1.0),dt/(dt-kp*period));
        }
        else kappa = 1.0;
    }

    //! adjust slow-down factor to integer times of periods for given dt
    /*! Obtain current slow-down factor
      @param [in] dt: physical time step (assume dt is constant before next update of kappa) without slow-down factor
      \return the corresponding orbital number for kappa
     */
    int adjustkappaPeriod(const Float dt) {
        if(is_used) {
            int kp = to_int(dt/(kappa*period))+1;
            kappa = dt/(kp*period);
            if(kappa<1.0) {
                kappa=1.0;
                return 0;
            }
            else return kp;
        }
        else {
            kappa = 1.0;
            return 0;
        }
    }


    // Get slow-down factor
    /*!
      \return get adjusted kappa by keeping phase corrected
    */
    Float getkappa() const {
        return kappa;
    }

    // Get original slow-down factor
    /*!
      \return kappa_origin
    */
    Float getkappaorg() const {
        return kappa_org;
    }

    // Get perturbation factor
    /*!
      \return the maximum perturbation force / internal force square record
     */
    Float getFratioSq() const {
        return f_ratio_sq_max;
    }

    // Get sd reference factor
    Float getSDRef() const {
        return kref;
    }

    //! Switcher of slow-down
    /*! @param[in] used: if true, switch on, else switch off
     */
    void switcher(const bool used) {
        is_used = used;
    }

    //! Dump class data
    /*! @param[in] fp: std::FILE pointer to dump data
     */
    void dump(std::FILE *fp) {
        fwrite(this, sizeof(chainslowdown),1,fp);
    }

    //! read class data
    /*! @param[in] fp: std::FILE pointer to read data
     */
    void read(std::FILE *fp) {
        size_t rcount = fread(this, sizeof(chainslowdown),1,fp);
        if (rcount<1) {
            std::cerr<<"Error: Data reading fails! requiring data number is 1, only obtain "<<rcount<<".\n";
            abort();
        }
    }

    //! backup real time and force ratio
    /*! @param[in] _bk: backup data array[3]
     */
    void backup(Float* _bk) {
        _bk[0] = kappa;
        _bk[1] = t_real;
        _bk[2] = t_record;
        _bk[3] = f_ratio_sq_max;
        _bk[4] = f_ratio_sq_peak;
    }

    //! restore real time and force ratio
    /*! @param[in] _bk: restore data array[3]
     */
    void restore(Float* _bk) {
        kappa  =   _bk[0];
        t_real =   _bk[1];
        t_record = _bk[2];
        f_ratio_sq_max = _bk[3];
        f_ratio_sq_peak = _bk[4];
    }
    

    //! print slowdown data
    /*! Print slowdown data 
      @param[in] fout: ofstream for printing
      @param[in] precision: printed precision for one variable
      @param[in] width: printing width for one variable
    */
    void print(std::ostream & fout, const int precision=15, const int width=23) {
        if (width<=0) {
            fout<<"Error: width should be larger than zero!\n";
            abort();
        }
        if(is_used) {
            fout<<std::setprecision(precision)
                <<"\n-------- slowdown ---------\n"
                <<"kappa: "<<std::setw(width)<<kappa<<std::endl
                <<"kappa_org: "<<std::setw(width)<<kappa_org<<std::endl
                <<"kappa_max: "<<std::setw(width)<<kappa_max<<std::endl
                <<"kref: "<<std::setw(width)<<kref<<std::endl
                <<"f_ratio_sq_max: "<<std::setw(width)<<f_ratio_sq_max<<std::endl
                <<"f_ratio_sq_peak: "<<std::setw(width)<<f_ratio_sq_peak<<std::endl
                <<"t_real: "<<std::setw(width)<<t_real<<std::endl
                <<"t_record: "<<std::setw(width)<<t_record<<std::endl
                <<"period: "<<std::setw(width)<<period<<std::endl;
        }
  }
    
    //! check whether it is used
    bool isUsed() const {
        return is_used;
    }
};

//! ARC class based on template class particle_
/*!
  Major class for ARC integration of few bodies
  
  It depend on the template class particle_. This particle_ class should contain public member functions for reading and writing mass, position and velocity (see sample in Particle::setPos(), Particle::setVel(), Particle::setMass(), Particle::getPos(), Particle::getVel(), Particle::getMass())

  The basic way to use ARC integration is shown as following:
  1. Construct a chain class with template class particle_ and a parameter controller of \ref ARC::chainpars. (The \ref ARC::chainpars should be configured first before doing integration. see its document for detail).
  2. Add existed particle 'A' (or a list of particles, or a chain type particle) into chain particle list chain.p using chain.addP(). Notice the chain.addP() only registers the particle A's memory address into chain.p and copy data into local array.
  3. Initialize chain with chain.init(). Notice this function is necessary to be called before integration. Also be careful that after this initialization, the positions and velocites of particles registered in \ref chain::p will be shifted from their original frame to their center-of-mass frame. The particle type member variable \ref chain.cm stores the center-of-mass data of these particles (the mass of \ref chain.cm is the total mass of all member particles).
  4. Call integration functions (chain.Leapfrog_step_forward() or chain.extrapolation_integration()). The former use only Leapfrog method and the latter use extrapolation method to obtain high accuracy of integration.
  5. After call integration functions, the particles are integrated to new time. Because in ARC method, the time is also integrated and cannot be predicted before integration, thus the iteration need to be done to get correct physical time you want (see detailed in chain.extrapolation_integration() document).
  6. Notice that after chain.init() and integration, the particles are always in the center-of-mass frame. If you want to shift them back to the original frame, the chain.center_shift_inverse() should be used. But after this function is used, you should use chain.center_shift() before the next integration. If you want the original partical address to be updated, copyback() should be called.

  Because of time now is an integrated variable, the time after integration cannot be predicted. 
  Thus if you want to stop the integration at a certain physical time, you need to use chain.extrapolation_integration() with dense output.
  To get better accuracy of physical time from intepolation, the 4k sequences (set in chainpars.setEXP()) is strongly suggested to be used. 
  If 4k sequences are used, the dense output method for GBS is used and the accuracy of time intepolation is close to the accuracy of integration.
  Please check the document of chain.extrapolation_integration() for detail.
 */
template <class particle_>
class chain: public particle_{
  typedef particle_ particle;
  chainlist<particle> p;    ///< particle list

  Float3 *X;  ///< relative position 
  Float3 *V;  ///< relative velocity
  Float3 *acc; ///< acceleration   
  Float3 *pf;  ///< perturber force
  Float3 *dWdr; ///< \partial Omega/ \partial rk
  int *list;   ///< chain index list
  ///< acc, pf, dWdr keep the same particle order as particle list p.

  //integration parameters=======================================//
  Float t;    ///< time
  Float Pt;    ///< Binding energy (time momentum)
  Float w;    ///< integrated time transformation function

  //template parameters==========================================//
  Float W;     ///< time transformation function
  Float Ekin;  ///< kinetic energy
  Float Pot;   ///< potential

  //number =======================================================//
  int num;      ///< total number of chain particles
  int nmax;     ///< maximum number 

  //monitor flags
  bool F_Pmod;     ///< indicate whether particle list is modified (true: modified)
  int  F_Porigin;  ///< indicate whether particle is shifted back to original frame (1: original frame: 0: center-of-mass frame; 2: only position is original frame)
  bool F_read;     ///< indicate whether read() funcion is used

  // information
  Float rmin2_, rmax2_; ///< min and max distance in the chain
  int rmin_index_, rmax_index_;  ///< min and max distance corresponding index in list (first component)

public:

  //particle cm;              ///< center mass particle
  chaininfo *info;          ///< chain information

  // slowdown control
  chainslowdown slowdown;      ///< chain slowdown controller

#ifdef ARC_PROFILE
  chainprofile profile;
#endif
  
    //! Constructor
  /*! Construct chain with allocated memory
      @param [in] n: maximum number of particles (will be used to allocate memory)
   */
  chain(const int n):   num(0), info(NULL), slowdown(){
    nmax=0;
    allocate(n);
  }

  //! Constructor
  /*! Construct chain without memory allocate, need to call allocate() later. 
   */
  chain(): num(0), nmax(0), F_Pmod(false), F_Porigin(1), F_read(false), info(NULL), slowdown() {}

  //! Allocate memory
  /*! Allocate memory for maximum particle number n
     @param [in] n: maximum number of particles
   */
  void allocate(const int n) {
    if (nmax) {
      std::cerr<<"Error: chain memory allocation is already done\n";
      abort();
    }
    nmax = n;
    X=new Float3[n-1];
    V=new Float3[n-1];
    acc=new Float3[n];
    pf=new Float3[n];
    dWdr=new Float3[n];
    list=new int[n];
    //p.allocate(n);
    F_Pmod=false;
    F_Porigin=1;
    F_read=false;
  }

  //! Clear function
  /*! Clear allocated memory and set maximum number of particle to zero
   */
  void clear() {
    if (nmax>0) {
      delete[] X;
      delete[] V;
      delete[] acc;
      delete[] pf;
      delete[] dWdr;
      delete[] list;
      num = 0;
      nmax = 0;
    }
    p.clear();
//    pext.clear();

    if (info) {
      delete info;
      info=NULL;
    }

    F_Pmod=false;
    F_Porigin=1;
    F_read=false;
#ifdef ARC_PROFILE
    profile.reset_tp();
#endif
  }

  chain& operator = (const chain& _c) {
      clear();
      *(particle*)this = *(particle*)&_c;
      allocate(_c.nmax);
      p = _c.p;

      int nrel=_c.num-1;
      for(int i=0; i<nrel; i++) for(int k=0; k<3; k++) X[i][k] = _c.X[i][k];
      for(int i=0; i<nrel; i++) for(int k=0; k<3; k++) V[i][k] = _c.V[i][k];
      for(int i=0; i<_c.num; i++) for(int k=0; k<3; k++) acc [i][k] = _c.acc [i][k];
      for(int i=0; i<_c.num; i++) for(int k=0; k<3; k++) pf  [i][k] = _c.pf  [i][k];
      for(int i=0; i<_c.num; i++) for(int k=0; k<3; k++) dWdr[i][k] = _c.dWdr[i][k];
      for(int i=0; i<_c.num; i++) list[i] = _c.list[i];
      
      t  = _c.t;
      Pt = _c.Pt;
      w  = _c.w;

      W = _c.W;
      Ekin = _c.Ekin;
      Pot  = _c.Pot;

      num = _c.num;
      
      F_Pmod = _c.F_Pmod;
      F_Porigin = _c.F_Porigin;
      F_read = _c.F_read;

      rmin2_ = _c.rmin2_;
      rmax2_ = _c.rmax2_;

      rmin_index_ = _c.rmin_index_;
      rmax_index_ = _c.rmax_index_;

      slowdown = _c.slowdown;

#ifdef ARC_PROFILE
      profile = _c.profile;
#endif
      return *this;
  }

  //! destructor
  ~chain() {
    if (nmax>0) {
      delete[] X;
      delete[] V;
      delete[] list;
      delete[] acc;
      delete[] pf;
      delete[] dWdr;
    }
    if (info) delete info;

//    p.clear();
//    pext.clear();
  }

private:
//  //! Update number of particles
//  /*! Update the number of particle (#num) due to current particle list number
//     @param [in] n: current particle number in particle list #p
//  */
//  void update_num(const int n) {
//    if (n>nmax) {
//      std::cerr<<"Error: particle number "<<n<<" is larger than Chain number limit "<<num<<std::endl;
//      abort();
//    }
//    else{
//      num = n; ///< Update Chain number
//    }
//  }

  //! generate the chain list 
  /*! generate the chain list (#list) by N^2 searching the particle list (#p)
  */
  void generate_list() {
    bool is_checked[nmax];
    for (int i=0; i<num; i++) is_checked[i] = false;

    // initial min/max distance recored
    const Float NUMERIC_FLOAT_MAX = std::numeric_limits<Float>::max();
    rmin2_ = NUMERIC_FLOAT_MAX;
    rmax2_ = 0.0;
    rmin_index_=0;
    rmax_index_=0;

    // initial first 
    int inext=0;
    list[0]=inext;
    is_checked[0] = true;

    for (int i=1; i<num; i++) {
      // initial rjk; mark checked particle
      // initial chain_mem
      int inow=inext;
    
      // make chain
      Float rmin2ij = NUMERIC_FLOAT_MAX;
      for (int j=1; j<num; j++) {
        if(is_checked[j]) continue;
        const Float* rj = p[j].getPos();
        const Float* ri = p[inow].getPos();
        Float dx = rj[0] - ri[0];
        Float dy = rj[1] - ri[1];
        Float dz = rj[2] - ri[2];
        Float dr2= dx*dx + dy*dy + dz*dz;
        if(dr2<rmin2ij) {
          rmin2ij = dr2;
          inext = j;
        }
      }
      // find rmin/rmax
      if(rmin2ij>rmax2_) {
          rmax2_ = rmin2ij;
          rmax_index_ = inow;
      }
      if(rmin2ij<rmin2_) {
          rmin2_ = rmin2ij;
          rmin_index_ = inow;
      }
      list[i]=inext;
      is_checked[inext] = true;
    }
  }

  //! Calculate relative position and velocity
  /*! Get chain member relative position #X and velocity #V based on #list
  */
  void calc_XV() {
    for (int i=0;i<num-1;i++) {
      const Float *ri1 = p[list[i+1]].getPos();
      const Float *ri = p[list[i]].getPos();
      X[i][0] = ri1[0] - ri[0];
      X[i][1] = ri1[1] - ri[1];
      X[i][2] = ri1[2] - ri[2];

      const Float *vi1 = p[list[i+1]].getVel();
      const Float *vi = p[list[i]].getVel();
      V[i][0] = vi1[0] - vi[0];
      V[i][1] = vi1[1] - vi[1];
      V[i][2] = vi1[2] - vi[2];
    }
  }

  //! initial perturbation acceleration #pf
  /*! Reset pf to zero
   */ 
  void initial_pf() {
    for (int i=0; i<num; i++) 
        for (int j=0; j<3; j++) pf[i][j] = 0.0;
  }

  //! calculate perturbation / inner acceleration square for two particles
  /*! the inner acceleration is calculated for all particles between i1 and i2 in the chain list
    @param[in] fforce: force function
    @param[in] i1: index in chain list for first particle
    @param[in] i2: index in chain list for second particle
    @param[in] pars: integration parameters
    \return fratio square
   */
  template<class extpar_>
  Float calcFratioInnerSq (pair_AW<particle, extpar_> fforce, const int i1, const int i2, extpar_ *pars) {
#ifdef ARC_DEBUG
      if(fforce==NULL) {
          std::cerr<<"Error: acceleration and time transformation calculation function is NULL!\n";
          abort();
      }
      if(i1<0||i2>=num) {
          std::cerr<<"Error: force calculation index range overflow, i1="<<i1<<" i2="<<i2<<std::endl;
          abort();
      }
#endif
      int ilst[2] = {i1, i2};
      Float3 acc_in[2];

      for (int iacc=0; iacc<2; iacc++) {
        int j = ilst[iacc];
        int lj = list[j];
        const particle *pj= &p[lj];

        for (int k=0; k<3; k++) {
            acc_in [iacc][k]=0.0; // reset Acceleration
        }

        for (int k=i1;k<=i2;k++) {
            if(k==j) continue;
            int lk = list[k];
            const particle *pk= &p[lk];
            const Float* xj = pj->getPos();
            Float3 xjk;

            if(k==j+1) {
                xjk[0] = X[j][0];
                xjk[1] = X[j][1];
                xjk[2] = X[j][2];
            }
            if(k==j-1) {
                xjk[0] = -X[k][0];
                xjk[1] = -X[k][1];
                xjk[2] = -X[k][2];
            }
            else if(k==j+2) {
                xjk[0] = X[j][0] + X[j+1][0];
                xjk[1] = X[j][1] + X[j+1][1];
                xjk[2] = X[j][2] + X[j+1][2];
            }
            else if(k==j-2) {
                xjk[0] = -X[k][0] - X[k+1][0];
                xjk[1] = -X[k][1] - X[k+1][1];
                xjk[2] = -X[k][2] - X[k+1][2];
            }
            else {
                const Float* xk = pk->getPos();
                xjk[0] = xk[0] - xj[0];
                xjk[1] = xk[1] - xj[1];
                xjk[2] = xk[2] - xj[2];
            }

            Float3 At,dWt_jk;
            Float Pot_jk,Wt_jk;

            // force calculation function from k to j
#ifdef ARC_WARN
            int stat = fforce(At, Pot_jk, dWt_jk, Wt_jk, xjk, *pj, *pk, pars);
            if (stat!=0&&info==NULL) {
                info = new chaininfo(num);
                info->status = stat;
                info->i1 = lj;
                info->i2 = lk;
                break;
            }
#else 
            fforce(At, Pot_jk, dWt_jk, Wt_jk, xjk, *pj, *pk, pars);
#endif

            // Acceleration
            acc_in[iacc][0] += At[0];
            acc_in[iacc][1] += At[1];
            acc_in[iacc][2] += At[2];
        }
      }
      Float3 daccp, daccin;
      Float daccp2, daccin2;
      daccin[0] = acc_in[0][0] - acc_in[1][0];
      daccin[1] = acc_in[0][1] - acc_in[1][1];
      daccin[2] = acc_in[0][2] - acc_in[1][2];

      daccp[0] = acc[i1][0] - acc[i2][0] - daccin[0];
      daccp[1] = acc[i1][1] - acc[i2][1] - daccin[1];
      daccp[2] = acc[i1][2] - acc[i2][2] - daccin[2];

      daccin2 = daccin[0]*daccin[0] + daccin[1]*daccin[1] + daccin[2]*daccin[2];
      daccp2 = daccp[0]*daccp[0] + daccp[1]*daccp[1] + daccp[2]*daccp[2];

      return daccp2/daccin2;
  }

  //! Calculate acceleration (potential) and transformation parameter
  /*! Get distance Matrix, acceleration, dm/dr, potential and transformation parameter
      based on particle masses in #p, using current #X, #V
      (notice the acceleration and dW/dr array index follow particle p to avoid additional shift when chain list change).

      @param [in] fforce: force function pointer (::pair_AW)
      @param [in] pars: extra parameters used in fforce
  */
  //      @param [in] resolve_flag: flag to determine whether to resolve sub-chain particles for force calculations. (defaulted false)
  template<class extpar_>
  void calc_rAPW (pair_AW<particle, extpar_> fforce, extpar_ *pars) {
#ifdef ARC_PROFILE
    profile.t_apw -= get_wtime();
#endif

#ifdef ARC_DEBUG
    if(fforce==NULL) {
        std::cerr<<"Error: acceleration and time transformation calculation function is NULL!\n";
        abort();
    }
#endif

    // reset potential and transformation parameter
    Float Pot_c  = 0.0;
    Float W_c  = 0.0;
    // Loop all particles in list
#ifdef USE_OMP
#pragma omp parallel for reduction(+:Pot_c), reduction(+:W_c)
#endif
    for (int j=0;j<num;j++) {
      int lj = list[j];
      const particle *pj= &p[lj];

      for (int k=0; k<3; k++) {
        acc [lj][k]=0.0; // reset Acceleration
        dWdr[lj][k]=0.0; // reset dW/dr
      }

      for (int k=0;k<num;k++) {
        if(k==j) continue;
        int lk = list[k];
        const particle *pk= &p[lk];
        const Float* xj = pj->getPos();
        Float3 xjk;

        if(k==j+1) std::memcpy(xjk,X[j],3*sizeof(Float));
        if(k==j-1) {
          xjk[0] = -X[k][0];
          xjk[1] = -X[k][1];
          xjk[2] = -X[k][2];
        }
        else if(k==j+2) {
          xjk[0] = X[j][0] + X[j+1][0];
          xjk[1] = X[j][1] + X[j+1][1];
          xjk[2] = X[j][2] + X[j+1][2];
        }
        else if(k==j-2) {
          xjk[0] = -X[k][0] - X[k+1][0];
          xjk[1] = -X[k][1] - X[k+1][1];
          xjk[2] = -X[k][2] - X[k+1][2];
        }
        else {
          const Float* xk = pk->getPos();
          xjk[0] = xk[0] - xj[0];
          xjk[1] = xk[1] - xj[1];
          xjk[2] = xk[2] - xj[2];
        }

        Float3 At,dWt_jk;
        Float Pot_jk,Wt_jk;
//        const Float mj=pj->getMass();
//        const Float mk=pk->getMass();

        // force calculation function from k to j
#ifdef ARC_WARN
        int stat = fforce(At, Pot_jk, dWt_jk, Wt_jk, xjk, *pj, *pk, pars);
        if (stat!=0&&info==NULL) {
          info = new chaininfo(num);
          info->status = stat;
          info->i1 = lj;
          info->i2 = lk;
          break;
        }
#else 
        fforce(At, Pot_jk, dWt_jk, Wt_jk, xjk, *pj, *pk, pars);
#endif

//        // resolve sub-chain
//        if(resolve_flag && p.isChain(lk)) {
//          chain<particle, int_par>*ck = p.getSub(lk);
//          // center shift to current frame
//          ck->center_shift_inverse_X();
//          const int cn = ck->p.getN();
//          Pot_jk = 0;
//          for (int i=0;i<3;i++) At[i]=0.0;
//          for (int i=0;i<cn;i++) {
//            Float Ptemp;
//            Float3 Atemp;
//            pars->pp_Ap(Atemp, Ptemp, xj, ck->p[i].getPos(), *pj, ck->p[i]);
// 
//            // Acceleration
//            At[0] += Atemp[0];
//            At[1] += Atemp[1];
//            At[2] += Atemp[2];
//            
//            // Potential
//            if (k>j) Pot_jk += Ptemp;
//          }
//          // center shift back
//          ck->center_shift_X();
//        }

        // Acceleration
        acc[lj][0] += At[0];
        acc[lj][1] += At[1];
        acc[lj][2] += At[2];

        // dW/dr
        dWdr[lj][0] += dWt_jk[0];
        dWdr[lj][1] += dWt_jk[1];
        dWdr[lj][2] += dWt_jk[2];

        if (k>j) {
          Pot_c += Pot_jk; // Potential energy
          W_c += Wt_jk;   // Transformation coefficient
        }
      }
      // add perturber force
      acc[lj][0] += slowdown.kappa*pf[lj][0];
      acc[lj][1] += slowdown.kappa*pf[lj][1];
      acc[lj][2] += slowdown.kappa*pf[lj][2];
    }

    // Write potential and w
    Pot = Pot_c;
    W = W_c;
#ifdef ARC_PROFILE
    profile.t_apw += get_wtime();
#endif
  }

  //! Calculate kinetic energy
  void calc_Ekin(){
    Ekin = 0.0;
    for (int i=0; i<num; i++) {
      const Float *vi=p[i].getVel();
      Ekin += 0.5 * p[i].getMass() * (vi[0]*vi[0]+vi[1]*vi[1]+vi[2]*vi[2]);
    }
  }

  //! Step forward of X
  /*! One step integration of #X 
     @param [in] dt: physical time step dt for #X
   */
  void step_forward_X(const Float dt) {
    // step forward relative X
    for (int i=0;i<num-1;i++) {
      X[i][0] += dt * V[i][0];
      X[i][1] += dt * V[i][1];
      X[i][2] += dt * V[i][2];
    }
  }

  //! Step forward of V
  /*! one step integration of V
     @param [in] dt: physical time step dt for #V
   */
  void step_forward_V(const Float dt) {
    // step forward V
    for (int i=0;i<num-1;i++) {
      int k = list[i];
      int k1 = list[i+1];
      V[i][0] += dt * (acc[k1][0]-acc[k][0]);
      V[i][1] += dt * (acc[k1][1]-acc[k][1]);
      V[i][2] += dt * (acc[k1][2]-acc[k][2]);
    }
  }

  //! Step forward of Pt and w
  /*! One step integration of Pt and w.
      - \f$Pt += dt * \sum ( - m_k * <v_k> \dot f_k)\f$ 
      - \f$w += dt * \sum ( dm/dr_k \dot v_k)\f$ 
     @param [in] dt: time step for V
     @param [in] ave_v: averaged velocity
   */
  void step_forward_Ptw(const Float dt, const Float3* ave_v, const bool calc_w_flag) {
    Float dPt = 0.0;
    Float dw  = 0.0;
    for (int i=0;i<num;i++) {
        dPt -= p[i].getMass() * (ave_v[i][0] * slowdown.kappa*pf[i][0] +
                                 ave_v[i][1] * slowdown.kappa*pf[i][1] +
                                 ave_v[i][2] * slowdown.kappa*pf[i][2]);
        
        if(calc_w_flag) dw += (ave_v[i][0] * dWdr[i][0] +
                               ave_v[i][1] * dWdr[i][1] +
                               ave_v[i][2] * dWdr[i][2]);
    }
    Pt += dt * dPt;
    w  += dt * dw;
  }

  //! resolve X and V
  /*! resolve relative #X, #V to physical x, v and calculated the averaged velocity of old and new values.
      Notice the center-of-mass particle mass in Chain.cm is used.
      The total mass of particles should be consistent with particle::getMass(). Otherwise update Chain.cm first.
      @param [out] ave_v: averaged velocity array (return values)
   */
  void resolve_XV(Float3* ave_v=NULL) {
    // backup old v
    if (ave_v!=NULL) {
      for (int i=0;i<num;i++) {
        const Float *vi = p[i].getVel();
        ave_v[i][0] = vi[0];
        ave_v[i][1] = vi[1];
        ave_v[i][2] = vi[2];
      }
    }
    // resolve current X,V
    // first member
    Float3 vc;
    Float3 xc;
    const int lk1=list[0];
    const Float  mk1 = p[lk1].getMass();
    const Float *rk1 = p[lk1].getPos();
    const Float *vk1 = p[lk1].getVel();
    xc[0] = mk1 * rk1[0];
    xc[1] = mk1 * rk1[1];
    xc[2] = mk1 * rk1[2];
    vc[0] = mk1 * vk1[0];
    vc[1] = mk1 * vk1[1];
    vc[2] = mk1 * vk1[2];
    // others
    for (int i=0;i<num-1;i++) {
      const int lk = list[i];
      const int lkn = list[i+1];
      const Float *rk = p[lk].getPos();
      p[lkn].setPos(rk[0] + X[i][0],
                    rk[1] + X[i][1],
                    rk[2] + X[i][2]);

      const Float *vk = p[lk].getVel();
      p[lkn].setVel(vk[0] + V[i][0],
                    vk[1] + V[i][1],
                    vk[2] + V[i][2]);

      //! center-of-mass position and velocity
      const Float mkn = p[lkn].getMass();
      const Float *rkn = p[lkn].getPos();
      const Float *vkn = p[lkn].getVel();
      xc[0] += mkn * rkn[0];
      xc[1] += mkn * rkn[1];
      xc[2] += mkn * rkn[2];
      vc[0] += mkn * vkn[0];
      vc[1] += mkn * vkn[1];
      vc[2] += mkn * vkn[2];
    }

    // calcualte center-of-mass position and velocity shift
    const Float over_mc = 1.0/particle::getMass();
    xc[0] *= over_mc;
    xc[1] *= over_mc;
    xc[2] *= over_mc;
    vc[0] *= over_mc;
    vc[1] *= over_mc;
    vc[2] *= over_mc;
    
    for (int i=0;i<num;i++) {
      // center-of-mass correction
      const Float *ri = p[i].getPos();
      p[i].setPos(ri[0] - xc[0],
                  ri[1] - xc[1],
                  ri[2] - xc[2]);
      const Float *vi = p[i].getVel();
      p[i].setVel(vi[0] - vc[0],
                  vi[1] - vc[1],
                  vi[2] - vc[2]);

      // calculate averaged velocities
      if (ave_v!=NULL) {
        ave_v[i][0] = 0.5 * (ave_v[i][0] + vi[0]);
        ave_v[i][1] = 0.5 * (ave_v[i][1] + vi[1]);
        ave_v[i][2] = 0.5 * (ave_v[i][2] + vi[2]);
      }
    }
  }

  //! Resolve X
  /*! Resolve relative #X to physical x (center-of-mass frame)
      Notice the center-of-mass particle mass in #Chain.cm is used.
      The total mass of particles should be consistent with particle::getMass(). Otherwise update Chain.cm first.
   */
  void resolve_X() {
    // resolve current X
    Float3 xc;
    // first member
    const int lk1=list[0];
    const Float  mk1 = p[lk1].getMass();
    const Float *rk1 = p[lk1].getPos();
    xc[0] = mk1 * rk1[0];
    xc[1] = mk1 * rk1[1];
    xc[2] = mk1 * rk1[2];
    // others
    for (int i=0;i<num-1;i++) {
      const int lk = list[i];
      const int lkn = list[i+1];
      const Float *rk = p[lk].getPos();
      p[lkn].setPos(rk[0] + X[i][0],
                    rk[1] + X[i][1],
                    rk[2] + X[i][2]);

      // center-of-mass position 
      const Float mkn = p[lkn].getMass();
      const Float *rkn = p[lkn].getPos();
      xc[0] += mkn * rkn[0];
      xc[1] += mkn * rkn[1];
      xc[2] += mkn * rkn[2];
    }

    // calcualte center-of-mass position shift
    const Float over_mc = 1.0/particle::getMass();
    xc[0] *= over_mc;
    xc[1] *= over_mc;
    xc[2] *= over_mc;
    
    for (int i=0;i<num;i++) {
      // center-of-mass correction
      const Float *ri = p[i].getPos();
      p[i].setPos(ri[0] - xc[0],
                  ri[1] - xc[1],
                  ri[2] - xc[2]);
    }
  }

  //! Resolve V
  /*! Resolve relative velocity #V to physical  v (center-of-mass frame)
    Notice the center-of-mass particle mass in Chain.cm is used.
    The total mass of particles should be consistent with particle::getMass(). Otherwise update Chain.cm first.
   */
  void resolve_V() {
    // resolve current V
    // first member
    Float3 vc;
    const int lk1=list[0];
    const Float  mk1 = p[lk1].getMass();
    const Float *vk1 = p[lk1].getVel();
    vc[0] = mk1 * vk1[0];
    vc[1] = mk1 * vk1[1];
    vc[2] = mk1 * vk1[2];
    // others
    for (int i=0;i<num-1;i++) {
      const int lk = list[i];
      const int lkn = list[i+1];
      const Float *vk = p[lk].getVel();
      p[lkn].setVel(vk[0] + V[i][0],
                    vk[1] + V[i][1],
                    vk[2] + V[i][2]);

      // center-of-mass position and velocity
      const Float mkn = p[lkn].getMass();
      const Float *vkn = p[lkn].getVel();
      vc[0] += mkn * vkn[0];
      vc[1] += mkn * vkn[1];
      vc[2] += mkn * vkn[2];
    }

    // calcualte center-of-mass position and velocity shift
    const Float over_mc = 1.0/particle::getMass();
    vc[0] *= over_mc;
    vc[1] *= over_mc;
    vc[2] *= over_mc;
    
    for (int i=0;i<num;i++) {
      // center-of-mass correction
      const Float *vi = p[i].getVel();
      p[i].setVel(vi[0] - vc[0],
                  vi[1] - vc[1],
                  vi[2] - vc[2]);
    }
  }

/*
  //! Perturber force calculation
  *! Get perturber force based on porturber list #pext
    @param [in] t: physical time for prediction
    @param [in] resolve_flag: whether resolve perturber member if it is a chain 
    \return flag: true: pertubers exist. false: no perturbers
  *
  bool pert_force(const Float t, const bool resolve_flag=false) {
#ifdef ARC_PROFILE
    profile.t_pext -= get_wtime();
#endif
    // safety check
    if (pars->pp_Ap==NULL) {
      std::cerr<<"Error: acceleration calculation function chainpars.pp_Ap is not set!\n";
      abort();
    }
    const int np = pext.getN();
    for (int i=0;i<num;i++) 
        for (int j=0;j<3;j++) pf[i][j] = 0.0;

    //predict
    for (int i=0; i<np;i++) {
        pert_predict(t, pext[j], 
    }

    for (int i=0;i<num;i++) {
        for (int j=0;j<np;j++) {
            Float3 At={0};
            Float Pt;
            const particle* pi=&p[i];
            const Float* xi=pi->getPos();
//          const Float  mi=pi->getMass();
          
            // check sub-chain system
            if (resolve_flag && pext.isChain(j)) {
                chain<particle, int_par>*cj = pext.getSub(j);
                // get center-of-mass position for shifting;
                const Float* xc=cj->particle::getPos();
                const int cn = cj->pext.getN();
                for (int k=0;k<cn;k++) {

                    Float3 xk;
                    std::memcpy(xk,cj->p[k].getPos(),3*sizeof(Float));
                    // shift position to current frame; to keep thread safety, original data are not modified
                    if (cj->isPorigin()==0) {
                        xk[0] += xc[0];
                        xk[1] += xc[1];
                        xk[2] += xc[2];
                    }
                    Float3 Atemp;
                    pars->pp_Ap(Atemp, Pt, xi, xk, *pi, cj->p[k], Int_pars);

                    // Acceleration
                    At[0] += Atemp[0];
                    At[1] += Atemp[1];
                    At[2] += Atemp[2];
                }
            }
            else {
                // perturber force
                pars->pp_Ap(At, Pt, xi, pext[j].getPos(), *pi, pext[j], Int_pars);
            }
          
            pf[i][0] += At[0];
            pf[i][1] += At[1];
            pf[i][2] += At[2];
        }
    }
#ifdef ARC_PROFILE
    profile.t_pext += get_wtime();
#endif
    return true;
  }
*/   

  //! Update #list order based on the relative distances
  /*! Update chain #list order based on current relative distance #X
    \return flag: if link is modified, return true
   */
  bool update_link(){
#ifdef ARC_PROFILE
    profile.t_uplink -= get_wtime();
#endif
    bool modified=false; // indicator
#ifdef ARC_DEEP_DEBUG        
    std::cerr<<"current:";
    for (int i=0;i<num;i++) std::cerr<<std::setw(4)<<list[i];
    std::cerr<<"\n";
#endif
    
    // create reverse index of link
    int rlink[nmax];
    int roldlink[nmax];
    for (int i=0;i<num;i++) rlink[list[i]] = i;
    std::memcpy(roldlink,rlink,num*sizeof(int));

    // backup previous link
    int listbk[nmax];
    std::memcpy(listbk,list,num*sizeof(int));

    // backup current X
    Float3 Xbk[nmax];
    std::memcpy(Xbk[0],X[0],(num-1)*3*sizeof(Float));
    
    // backup current V
    Float3 Vbk[nmax];
    std::memcpy(Vbk[0],V[0],(num-1)*3*sizeof(Float));

    // create mask to avoid dup. check;
    bool mask[nmax];
    for (int i=0;i<num;i++) mask[i] = false;

    const Float NUMERIC_FLOAT_MAX = std::numeric_limits<Float>::max();

    // initial min/max distance recored
    rmin2_ = NUMERIC_FLOAT_MAX;
    rmax2_ = 0.0;
    rmin_index_=0;
    rmax_index_=0;

    // find link
    for (int k=0;k<num-1;k++) {
      int lk  = list[k];
       mask[lk] = true;
      int lkn = list[k+1];
      // possible new index
      int lku = lkn;
      // calculate distance
      Float rmin2ij = NUMERIC_FLOAT_MAX;
      for (int j=0;j<num;j++) {
        if (mask[j]||j==lk) continue;
        const Float* xk = p[lk].getPos();
        const Float* xj = p[j].getPos();
        Float xjk = xk[0] - xj[0];
        Float yjk = xk[1] - xj[1];
        Float zjk = xk[2] - xj[2];
        Float rjk2 = xjk*xjk + yjk*yjk + zjk*zjk;
        if (rjk2<rmin2ij) {
          lku=j;
          rmin2ij = rjk2;
        }
      }
      // find rmin/rmax
      if(rmin2ij>rmax2_) {
#ifdef ARC_DEBUG
          assert(rmin2ij!=NUMERIC_FLOAT_MAX);
#endif
          rmax2_ = rmin2ij;
          rmax_index_ = k;
      }
      if(rmin2ij<rmin2_) {
          rmin2_ = rmin2ij;
          rmin_index_ = k;
      }

      if (lku!=lkn) {
#ifdef ARC_DEEP_DEBUG        
        std::cerr<<"Switch: "<<k<<" new: "<<lku<<" old: "<<lkn<<std::endl;
        for (int i=0;i<num;i++) std::cerr<<std::setw(4)<<list[i];
        std::cerr<<"\n Rlink";
        for (int i=0;i<num;i++) std::cerr<<std::setw(4)<<rlink[i];
        std::cerr<<"\n";
#endif
        modified=true;
        // shift two index in the list
        list[rlink[lku]] = lkn;
        rlink[lkn] = rlink[lku];
        list[k+1] = lku;
        rlink[lku] = k+1;
        mask[lku] = true;
#ifdef ARC_DEEP_DEBUG        
        for (int i=0;i<num;i++) std::cerr<<std::setw(4)<<list[i];
        std::cerr<<"\n Rlink";
        for (int i=0;i<num;i++) std::cerr<<std::setw(4)<<rlink[i];
        std::cerr<<"\n";
#endif
      }

      if (lk!=listbk[k]||lku!=listbk[k+1]) {
        // update X and V
        // left boundary
        int rlk = roldlink[lk];
        if (rlk<k) {
          for (int j=rlk;j<k;j++) {
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"Add V["<<j<<"] to V["<<k<<"]\n";
#endif
            X[k][0] += Xbk[j][0];
            X[k][1] += Xbk[j][1];
            X[k][2] += Xbk[j][2];

            V[k][0] += Vbk[j][0];
            V[k][1] += Vbk[j][1];
            V[k][2] += Vbk[j][2];
          }
        }
        else if (rlk>k) {
          for (int j=k;j<rlk;j++) {
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"Minus V["<<j<<"] from V["<<k<<"]\n";
#endif
            X[k][0] -= Xbk[j][0];
            X[k][1] -= Xbk[j][1];
            X[k][2] -= Xbk[j][2];

            V[k][0] -= Vbk[j][0];
            V[k][1] -= Vbk[j][1];
            V[k][2] -= Vbk[j][2];
          }
        }
        // right boundary
        rlk = roldlink[lku];
        if (rlk<k+1) {
          for (int j=rlk;j<k+1;j++) {
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"Minus V["<<j<<"] from V["<<k<<"]\n";
#endif
            X[k][0] -= Xbk[j][0];
            X[k][1] -= Xbk[j][1];
            X[k][2] -= Xbk[j][2];

            V[k][0] -= Vbk[j][0];
            V[k][1] -= Vbk[j][1];
            V[k][2] -= Vbk[j][2];
          }
        }
        else if (rlk>k+1) {
          for (int j=k+1;j<rlk;j++) {
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"Add V["<<j<<"] to V["<<k<<"]\n";
#endif
            X[k][0] += Xbk[j][0];
            X[k][1] += Xbk[j][1];
            X[k][2] += Xbk[j][2];

            V[k][0] += Vbk[j][0];
            V[k][1] += Vbk[j][1];
            V[k][2] += Vbk[j][2];
          }
        }
      }
    }

#ifdef ARC_DEEP_DEBUG    
    if(modified) print(std::cerr);
#endif
    
#ifdef ARC_PROFILE
    profile.t_uplink += get_wtime();
#endif
    return modified;
  }

  //! Center of mass shift 
  /*! Shift positions and velocities of N (#num) particles (#p) based on their center-of-mass, write center-of-mass particle to chain
  */
  void center_shift_init() {
    // center mass
    Float3 cmr={};
    Float3 cmv={};
    Float  cmm=0;
    for (int i=0;i<num;i++) {
      const Float *ri = p[i].getPos();
      const Float *vi = p[i].getVel();
      const Float mi = p[i].getMass();
      cmr[0] += ri[0] * mi;
      cmr[1] += ri[1] * mi;
      cmr[2] += ri[2] * mi;

      cmv[0] += vi[0] * mi;
      cmv[1] += vi[1] * mi;
      cmv[2] += vi[2] * mi;

      cmm += mi;
    }
    cmr[0] /= cmm; 
    cmr[1] /= cmm; 
    cmr[2] /= cmm; 

    cmv[0] /= cmm; 
    cmv[1] /= cmm; 
    cmv[2] /= cmm;

    particle::setMass(cmm);
    particle::setPos(cmr[0],cmr[1],cmr[2]);
    particle::setVel(cmv[0],cmv[1],cmv[2]);

    // shifting
    for (int i=0;i<num;i++) {
      const Float *ri = p[i].getPos();
      const Float *vi = p[i].getVel();
      p[i].setPos(ri[0] - cmr[0],
                  ri[1] - cmr[1],
                  ri[2] - cmr[2]);
      p[i].setVel(vi[0] - cmv[0],
                  vi[1] - cmv[1],
                  vi[2] - cmv[2]);
    }
  }

  //! Inversed center of mass shift for #X
  /*! Shift back positions of N (num) particles (p) based on chain center-of-mass
  */
  void center_shift_inverse_X() {
    if (F_Porigin>0) {
      std::cerr<<"Warning: particles are already in original frame!\n";
    }
    else {
      const Float *rc = particle::getPos();
      for (int i=0;i<num;i++) {
        const Float *ri = p[i].getPos();
        p[i].setPos(ri[0] + rc[0],
                    ri[1] + rc[1],
                    ri[2] + rc[2]);
      }
      F_Porigin=2;
    }
  }

  //! Center of mass shift for #X
  /*! Shift positions and velocities of N (num) particles (p) based on chain center-of-mass
  */
  void center_shift_X() {
    if (F_Porigin==0) {
      std::cerr<<"Warning: particles are already in the center-of-mass frame!\n";
    }
    else if (F_Porigin==1) {
      std::cerr<<"Error: particles are in original frame, please use center_shift instead!\n";
      abort();
    }
    else {
      const Float *rc = particle::getPos();
      for (int i=0;i<num;i++) {
        const Float *ri = p[i].getPos();
        p[i].setPos(ri[0] - rc[0],
                    ri[1] - rc[1],
                    ri[2] - rc[2]);
      }
      F_Porigin=0;
    }
  }


  //! Middle difference calculator
  /*! Central difference cumulative calculator for time #t
     If \a i is 0, the initial values will be reset to zero, and for the same n, this function need to be called n+1 times to complete the n'th order difference
    @param [in] dpoly: two dimensional array to storage the difference of dataset for different order. Array size is [\a nmax][dsize]. dsize = 1 due to one point
                       If dpoly is NULL, no calculation will be done.
    @param [in] nmax: maximum difference level (count from 1)
    @param [in] i: current position of dataset corresponding to the binomial coefficients (count from 0 as starting point)
    @param [in] ndiv: substep number
    @param [in] pars: chainpars controller
  */
    void mid_diff_calc(Float2 *dpoly, const int nmax, const int i, const int ndiv, chainpars &pars) {
    // safety check
    if (dpoly!=NULL) {
      // difference level should not exceed the point number
      if (2*nmax>(int)ndiv) {
        std::cerr<<"Error: maximum difference order "<<nmax<<" *2 > total step number "<<ndiv<<"!\n";
        abort();
      }
      // for middle difference, odd substep number is not allown
      if(ndiv%2) {
        std::cerr<<"Error: middle difference cannot allow odd substep number ("<<ndiv<<")!\n";
        abort();
      }
      if (i>=ndiv/2-nmax&&i<=ndiv/2+nmax) {
//        if (!tflag) {
//          const int dsize=6*num-3;
//          //initial dpoly to zero
//          for (int j=0; j<nmax; j++) if(i==0) std::memset(dpoly[j],0,dsize*sizeof(Float));
//        }
//        else
        if(i==0) for (int j=0; j<nmax; j++) dpoly[j][0] = 0.0;

        // dt/ds
        Float dts=calc_dt_X(1.0,pars);
        
        int** binI = pars.bin_index;
        // formula delta^n f(x) = sum_ik=0,n (-1)^(n-ik) (n n-ik) f(x+2*ik*h)
        for (int j=0; j<nmax; j++) {
          if ((i%2&&j%2)||(i%2==0&&j%2==0)) {
            // n=j+1 indicate the difference degree (first difference is j=0)
            const int n=j+1;
            // for even and odd difference level n, different i points are used
            // ndiv/2: middle point; n/2: left edge point shift, (should be Float since only odd points are used);
            int ik= i-(ndiv/2-n);
            // ik should be limited for diferent level of difference
            if (ik>=0&&ik<=2*n) {
              int nk= n-ik/2;  // n-ik
              Float coff = ((nk%2)?-1:1)*binI[n][nk];
//#ifdef ARC_DEEP_DEBUG
//              std::cerr<<"Poly_coff: n= "<<n<<"; i="<<i<<"; ik="<<ik<<"; coff="<<coff<<std::endl;
//#endif
              dpoly[j][0] += coff * dts;
//              if (!tflag) {
//                dpoly[j][1] += coff * Pt;
//                dpoly[j][2] += coff * w;
//                for (int k=0; k<num-1; k++) {
//                  for (int kk=0; kk<3; kk++) {
//                    dpoly[j][3*(1+k)+kk] += coff * X[k][kk];
//                    dpoly[j][3*(num+k)+kk] += coff * V[k][kk];
//                  }
//                }
//              }
            }
          }
        }
      }
    }
  }


  //! Edge difference calculator
  /*! Left forward and right backward difference cumulative calculator for time #t
      If i is 0, the array will be reset to zero, and for the same n, this function need to be called n+1 times (i=0...n+1) to complete the n'th order difference.
      @param [out] dpoly: two dimensional array to storage the results. Array size is [\a nmax][dsize].  dsize =2 because of two points.
                          first [] indicate different level of differences, second [] indicate the left and right difference of time #t
                          If dpoly is NULL, no calculation will be done.    
      @param [in] nmax: maximum difference level (count from 1)
      @param [in] i: current position of data corresponding to the binomial coefficients (count from 0 as starting point)              
      @param [in] ndiv: substep number
      @param [in] pars: chainpars controller
   */
    void edge_diff_calc(Float2 *dpoly, const int nmax, const int i, const int ndiv, const chainpars &pars) {
    if (dpoly!=NULL) {
      // safety check
//      if (!tflag) {
//        const int dsize=12*num-6;
//        // i indicate the position, i=0 means x0
//        if(i==0) for (int j=0; j<nmax; j++) std::memset(dpoly[j],0,dsize*sizeof(Float)); // reset data array at i=0
//      }
//      else
      if(i==0) for (int j=0; j<nmax; j++) {
          dpoly[j][0]=0.0;
          dpoly[j][1]=0.0;
        }
      
      if (nmax>ndiv) {
        std::cerr<<"Error: maximum difference order "<<nmax<<" > total step number "<<ndiv<<"!\n";
        abort();
      }

      if (i<=nmax||i>=ndiv-nmax) {
        // dt/ds
        Float dts=calc_dt_X(1.0,pars);

        int** binI = pars.bin_index;
        for (int j=0; j<nmax; j++) {
          // j+1 indicate the difference degree, count from 1 (first difference)
          const int n = j+1;
              
          // left edge, forward difference: (-1)^(n-i) (n n-i) f(x0+i*h) (i from 0 to n)
          int ileft = (int)(n-i);   // n-i
          if (ileft>=0) {
            Float coff = ((ileft%2)?-1:1)*binI[n][ileft];
            dpoly[j][0] += coff * dts;
//            if (!tflag) {
//              dpoly[j][1] += coff * Pt;
//              dpoly[j][2] += coff * w;
//              for (int k=0; k<num-1; k++) {
//                for (int kk=0; kk<3; kk++) {
//                  dpoly[j][3*(1+k)+kk] += coff * X[k][kk];
//                  dpoly[j][3*(num+k)+kk] += coff * V[k][kk];
//                }
//              }
//            }
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"Poly left: n="<<n<<" ik="<<ileft<<" i="<<i<<" coff="<<coff<<" t="<<t<<std::endl;
#endif
          }

          // right edge, backward difference: (-1)^(n-i) (n n-i) f(xn-(n-i)*h) (i from 0 to n)
          int ishift = ndiv-n;
          int iright= ileft+ishift;     // n-i
          if (i>=ishift) {
            Float coff = ((iright%2)?-1:1)*binI[n][iright];
//            if (!tflag) {
//              const int ir = 6*num-3;
//              dpoly[j][0+ir] += coff * t;
//              dpoly[j][1+ir] += coff * Pt;
//              dpoly[j][2+ir] += coff * w;
//              for (int k=0; k<num-1; k++) {
//                for (int kk=0; kk<3; kk++) {
//                  dpoly[j][3*(1+k)+kk+ir] += coff * X[k][kk];
//                  dpoly[j][3*(num+k)+kk+ir] += coff * V[k][kk];
//                }
//              }
//            } else
            dpoly[j][1] += coff * dts;
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"                 Poly right: n="<<n<<" ik="<<iright<<" i="<<i<<" coff="<<coff<<std::endl;
#endif
          }
        }
      }
    }
  }

  //! diff_dev_calc
  /*! Calcualte derivates from differences: \d^(n)f/ h^n
    @param [in,out] dpoly: two dimensional storing differences, will be updated to derivates. Array size is [nmax][dsize]. 
                          first [] indicate different level of differences, second [] indicate the difference of data. 
                          If dpoly is NULL, no calculation will be done.
    @param [in] h: step size
    @param [in] nmax: maximum difference order
    @param [in] dsize: number of data in dataset
   */
  void diff_dev_calc(Float2 *dpoly, const Float h, const int nmax, const int dsize) {
    Float hn = h;
    // loop difference order from 1 to nmax
    for (int i=0; i<nmax; i++) {
#ifdef ARC_DEEP_DEBUG
      std::cerr<<"Diff order "<<i<<"; h="<<hn<<"; nmax="<<nmax<<std::endl;
#endif
      for (int j=0; j<dsize; j++) dpoly[i][j] /= hn;
      hn *= h;
    }
  }

  //! update slow-down perturbation force
  /*! Update slow-down perturbation maximum force
   */
  void updateSlowDownFpert() {
      Float fratiosqmax=0.0;
      for (int i=0; i<num-1; i++) {
          int k = list[i];
          int k1 = list[i+1];
          //Float mk = p[k].getMass();
          //Float mk1 = p[k1].getMass();
          Float fp[3] = {pf[k1][0]-pf[k][0],
                         pf[k1][1]-pf[k][1],
                         pf[k1][2]-pf[k][2]};
          //Float fcp[3] = {mk1*pf[k1][0]-mk*pf[k][0],
          //                mk1*pf[k1][1]-mk*pf[k][1],
          //                mk1*pf[k1][2]-mk*pf[k][2]};
          Float fin[3] = {acc[k1][0]-acc[k][0] - slowdown.kappa*fp[0],
                          acc[k1][1]-acc[k][1] - slowdown.kappa*fp[1],
                          acc[k1][2]-acc[k][2] - slowdown.kappa*fp[2]};
          Float fp2 = fp[0]*fp[0] + fp[1]*fp[1] + fp[2]*fp[2];
          Float fin2= fin[0]*fin[0] +fin[1]*fin[1] +fin[2]*fin[2];
          //Float fcp2 = fcp[0]*fcp[0] + fcp[1]*fcp[1] + fcp[2]*fcp[2];
          //Float fc2 = pf[k1][0]*pf[k1][0] + pf[k1][1]*pf[k1][1] + pf[k1][2]*pf[k1][2];
          //Float m12 = mk*mk1;
          //Float mt= mk+mk1;
          fratiosqmax = std::max(fratiosqmax,fp2/fin2);
      }
      slowdown.updateFRatioSqRange(fratiosqmax);
  }

  //! initial slow-down perturbation force
  /*! initial slow-down perturbation force
   */
  void initialSlowDownFpert() {
      Float fratiosqmax=0.0;
      for (int i=0; i<num-1; i++) {
          int k = list[i];
          int k1 = list[i+1];
          //Float mk = p[k].getMass();
          //Float mk1 = p[k1].getMass();
          Float fp[3] = {pf[k1][0]-pf[k][0],
                         pf[k1][1]-pf[k][1],
                         pf[k1][2]-pf[k][2]};
          //Float fcp[3] = {mk1*pf[k1][0]-mk*pf[k][0],
          //                mk1*pf[k1][1]-mk*pf[k][1],
          //                mk1*pf[k1][2]-mk*pf[k][2]};
          Float fin[3] = {acc[k1][0]-acc[k][0] - slowdown.kappa*fp[0],
                          acc[k1][1]-acc[k][1] - slowdown.kappa*fp[1],
                          acc[k1][2]-acc[k][2] - slowdown.kappa*fp[2]};
          Float fp2 = fp[0]*fp[0] + fp[1]*fp[1] + fp[2]*fp[2];
          Float fin2= fin[0]*fin[0] +fin[1]*fin[1] +fin[2]*fin[2];
          //Float fcp2 = fcp[0]*fcp[0] + fcp[1]*fcp[1] + fcp[2]*fcp[2];
          //Float fc2 = pf[k1][0]*pf[k1][0] + pf[k1][1]*pf[k1][1] + pf[k1][2]*pf[k1][2];
          //Float m12 = mk*mk1;
          //Float mt= mk+mk1;
          fratiosqmax = std::max(fratiosqmax,fp2/fin2);
      }
      slowdown.initialFRatioSqRange(fratiosqmax);
  }


//  // collect accident information
//  void info_collection(const int status, const int intcount=-1, const Float perr=-1.0, const Float perr0=-1.0, const Float eerr=-1.0, const Float eerr0=-1.0, const Float terr=-1.0, const int i1=-1, const int i2=-1, const int inti=-1) {
//    info = new chaininfo(num);
//    info->status = 1;
//    info->intcount = intcount+1;
//    info->perr = perr;
//    info->perr0 = perr0;
//    info->eerr = eerr;
//    info->eerr0 = eerr0;
//    info->terr = terr;
//    info->i1 = i1;
//    info->i2 = i2;
//    info->inti = inti;
//    backup(info->data);
//  }

public:
  //! Find the most strongly interacted pair
  /*! Find the most strongly interacted pair.
    First the dacc[i] = acc[i+1]-acc[i] (i=0, num-1) is calculated, then select the maximum dacc[i], the corresponding two particles are selected as the strong pair. Their indice are written to pindex.
    @param [in] pindex: two element array used for storing pair index.
    \return the acceleraction difference of this pair
   */
  Float find_strong_pair(int pindex[2]) {
    int k=0;
    Float accm = 0;
    for (int i=0; i<num-1; i++) {
      const Float accx = acc[i+1][0]-acc[i][0];
      const Float accy = acc[i+1][1]-acc[i][1];
      const Float accz = acc[i+1][2]-acc[i][2];
      Float acci = accx*accx + accy*accy + accz*accz;
      if (accm<acci) {
        accm = acci;
        k = i;
      }
    }
    pindex[0] = list[k];
    pindex[1] = list[k+1];

    return sqrt(accm);
  }

  //! Find the closest pair
  /*! Find the closest pair using relative positions
    @param [in] pindex: two element array used for storing pair index.
    \return the relative distance of this pair
   */
  Float find_closest_pair(int pindex[2]) {
    int k=0;
    Float Xm = std::numeric_limits<Float>::max();
    for (int i=0; i<num-1; i++) {
      Float Xi2 = X[i][0]*X[i][0] + X[i][1]*X[i][1] + X[i][2]*X[i][2];
      if (Xm > Xi2) {
        Xm = Xi2;
        k = i;
      }
    }
    pindex[0] = list[k];
    pindex[1] = list[k+1];

    return sqrt(Xm);
  }

  //! get mininum distance and corresponding index
  /*! Get minimum distance (square) in the chain and the first component index in the link list
    @param[out] _rmin2: minimum distance square
    @param[out] _rmin_index: corresponding first component index in chain list
   */
  void getRminIndex(Float& _rmin2, int& _rmin_index) {
      _rmin2 = rmin2_;
      _rmin_index = rmin_index_;
  }

  //! get maxinum distance and corresponding index
  /*! Get maximum distance (square) in the chain and the first component index in the link list
    @param[out] _rmax2: maximum distance square
    @param[out] _rmax_index: corresponding first component index in chain list
   */
  void getRmaxIndex(Float& _rmax2, int& _rmax_index) {
      _rmax2 = rmax2_;
      _rmax_index = rmax_index_;
  }

  //! get whether two pair is leaving each other or coming 
  /*! Check X*V of the pair sign, the left and right c.m. are used
    @param[in] _first_index: the pair index to check (first component)
    \return true: go away, otherwise income
   */
  bool getDirection(const int _first_index) {
      const int i1 = _first_index;
      if(i1>=num-1) {
          std::cerr<<"Error, getDirection index overflow, index="<<i1<<" particle num="<<num<<std::endl;
          abort();
      }
      if(num==2) {
          Float xv2 = X[i1][0]*V[i1][0] + X[i1][1]*V[i1][1] + X[i1][2]*V[i1][2];
          return (xv2>0);
      }
      else {
          Float3 xcm1={0},vcm1={0};
          for (int i=0; i<=i1; i++) {
              const int k=list[i];
              const Float* xk = p[k].getPos();
              const Float* vk = p[k].getVel();
              const Float  mk = p[k].getMass();
              for (int j=0; j<3; j++) {
                  xcm1[j] += mk*xk[j];
                  vcm1[j] += mk*vk[j];
              }
          }
          Float3 xcm2={0}, vcm2={0};
          for (int i=i1+1; i<num; i++) {
              const int k=list[i];
              const Float* xk = p[k].getPos();
              const Float* vk = p[k].getVel();
              const Float  mk = p[k].getMass();
              for (int j=0; j<3; j++) {
                  xcm2[j] += mk*xk[j];
                  vcm2[j] += mk*vk[j];
              }
          }
          Float xv2 = (xcm2[0]-xcm1[0])*(vcm2[0]-vcm1[0])
              + (xcm2[1]-xcm1[1])*(vcm2[1]-vcm1[1])
              + (xcm2[2]-xcm1[2])*(vcm2[2]-vcm1[2]);
          return (xv2>0);
      }
  }

  //! get acceleration ratio of perturbation and inner at one break index for both sides
  /*! if one side is single, return value 1 for ratio for that side
    @param[out]: _fratiosq_left: perturbation/ inner acceleration square for left side
    @param[out]: _fratiosq_right: perturbation/ inner acceleration square for right side
    @param[in]: _break_index: first index to break the two sides for chain
   */
    template<class extpar_>
    void getFratioInnerSq(Float& _fratiosq_left, Float& _fratiosq_right, const int _break_index,  const chainpars &_pars, extpar_ *_int_pars) {
      const int ibreak = _break_index;
      if(ibreak>=num-1) {
          std::cerr<<"Error, getFratioInnerSq index overflow, index="<<ibreak<<" particle num="<<num<<std::endl;
          abort();
      }
      if(num==2) {
          _fratiosq_left = _fratiosq_right = 1.0;
      }
      else {
          pair_AW<particle,extpar_> f = reinterpret_cast<pair_AW<particle,extpar_>>(_pars.pp_AW);
//#ifdef ARC_DEBUG
//          if(std::type_index(typeid(f))!=*_pars.pp_AW_type) {
//              std::cerr<<"Error: acceleration function type not matched the data type\n";
//              abort();
//          }
//#endif
          // left
          if(ibreak>0) _fratiosq_left=calcFratioInnerSq(f, 0, ibreak, _int_pars);
          else _fratiosq_left = 1.0;
          // right
          if(num>ibreak+2) _fratiosq_right=calcFratioInnerSq(f, ibreak+1, num-1, _int_pars);
          else _fratiosq_right = 1.0;
      }
    }

  //! get particle original address in the chain list order
  /*! 
    @param[out] _list particle address array to store the results
    \return number of particles in chain
  */
  template <class Tp>
  int getPAdrChain(Tp* _list[]) {
      particle** ptr=p.getPAdr();
      for (int i=0; i<num; i++) _list[i] = (Tp*)ptr[list[i]];
      return num;
  }
  
  //! Calculate physical time step for X
  /*! Calculate physical time step dt for #X based on ds
     @param [in] ds:  integration step size (not physical time step) 
     @param [in] pars: chainpars controller
     \return     dt:  physical integration time step for #X
  */
  Float calc_dt_X(const Float ds, const chainpars &pars) {
    // determine the physical time step
    Float dt = ds / (pars.alpha * (Ekin + Pt) + pars.beta * w + pars.gamma);
//#ifdef ARC_WARN
//    if (abs(dt) < pars.dtmin && info==NULL) {
//      info=new chaininfo(num);
//      info->status = 4;
//      info->subdt = dt;
//    }
//#endif
    return dt;
  }
  
  //! Calculate physical time step for V
  /*! Calculate physical time step dt for #V based on ds
     @param [in] ds:  step size s (not physical time step) 
     @param [in] pars: chainpars controllers
     \return     dt:  physical integration time step for #V
  */
    Float calc_dt_V(const Float ds, const chainpars &pars) {
    // determine velocity integration time step
    return ds / (pars.gamma - pars.alpha * Pot + pars.beta * W);
  }
  

  //! Calculate next step approximation based on min(X/(gV),V/(gA))
  /*!
    @param[in] pars: chainpars controllers
    \return: approximation of step size ds
  */
  Float calc_next_step_XVA(const chainpars &pars) {
    Float dsXV=std::numeric_limits<Float>::max();
    Float dsVA=std::numeric_limits<Float>::max();
    for (int i=0; i<num-1; i++) {
      Float r = X[i][0]*X[i][0]+X[i][1]*X[i][1]+X[i][2]*X[i][2];
      Float v = V[i][0]*V[i][0]+V[i][1]*V[i][1]+V[i][2]*V[i][2];
      Float a = acc[i][0]*acc[i][0]+acc[i][1]*acc[i][1]+acc[i][2]*acc[i][2];
      dsXV = std::min(r/v,dsXV);
      dsVA = std::min(v/a,dsVA);
    }
    return pars.auto_step_eps*std::min(sqrt(dsXV)/calc_dt_X(1.0,pars),sqrt(dsVA)/calc_dt_V(1.0,pars));
  }

  //! Calculate next step approximation based on custom defined timescale for two-body system
  /*!
    The user-defined two-body timescale function with ::ARC::pair_T type will be performed on all neighbor pairs in chain.
    Then the minimum timescale \f$ T_m\f$ is selected to calculate next step size: \f$ ds = eps T_m |Pt|\f$, where \f$eps\f$ is ::chainpars.auto_step_eps set in chainpars.setAutoStep().
    @param [in] pars: chainpars controllers
    @param [in] extpars: extra parameters used in pt
    \return approximation of step size ds
   */
  template<class chainpars_, class extpar_>
  Float calc_next_step_custom(const chainpars_& pars, extpar_* extpars) {
      pair_T<particle,extpar_> pp_T = reinterpret_cast<pair_T<particle,extpar_>>(pars.pp_T);
#ifdef ARC_DEBUG
      if(std::type_index(typeid(pp_T))!=*pars.pp_T_type) {
          std::cerr<<"Error: pair timescale function type not matched!\n";
          abort();
      }
      //safety check
      if (pp_T==NULL) {
          std::cerr<<"Error: two-body timescale calculator function chainpars.pp_T is not set\n";
          abort();
      }
#endif

    Float perim = std::numeric_limits<Float>::max();
    for (int i=0; i<num-1; i++) {
      const Float peri=pp_T(p[list[i]].getMass(),p[list[i+1]].getMass(),X[i],V[i],extpars);
      if (perim>peri) perim = peri;
    }
//    std::cerr<<"perim = "<<perim<<" auto_step_eps"<<pars.auto_step_eps<<" Pt "<<Pt<<std::endl;
    return pars.auto_step_eps*perim*abs(Pt);
  }

  //! Add particle
  /*! Add one particle (address pointer) into particle list #p and copy it (see ARC::chainlist.add())
     @param [in] a: new particle
   */
  template <class Tp>
  void addP(Tp &a) {
    if (!p.is_alloc()) p.allocate(nmax);
    if (F_Porigin!=1) std::cerr<<"Warning!: particle list are (partically) in the center-of-mass frame, dangerous to add new particles!\n";
    p.add(a);
    F_Pmod=true;
  }
  
  ////! Add chain as a particle in #p
  ///*! Add one chain (address pointer) into particle list #p (see ARC::chainlist.add())
  //  @param [in] a: new chain particle
  // */
  //void addP(chain<particle> &a) {
  //  if (F_Porigin!=1) std::cerr<<"Warning!: particle list are (partically) in the center-of-mass frame, dangerous to add new particles!\n";
  //  p.add(a);
  //  F_Pmod=true;
  //}
  
  //! Add a list of particle
  /*! Add a list of particles addresses and copy (see ARC::chainlist.add())
    @param [in] n: number of particles need to be added
    @param [in] a: array of new particles
   */
  template<class Tp>
  void addP(const int n, Tp a[]) {
    if (!p.is_alloc()) p.allocate(nmax);
    if (F_Porigin!=1) std::cerr<<"Warning!: particle list are (partically) in the center-of-mass frame, dangerous to add new particles!\n";
    p.add(n,a);
    F_Pmod=true;
  }

  //! Link a list of particles without coping
  /*! Link a list of particles without copy. The particles shoud have same data type of particle_ and saved in array.
    @param [in] n: number of particles need to be added
    @param [in] a: array of new particles
   */
  void linkP(const int n, particle a[]) {
    if (F_Porigin!=1) std::cerr<<"Warning!: particle list are (partically) in the center-of-mass frame, dangerous to add new particles!\n";
    p.load(n,a);
    F_Pmod=true;
  }

  //! remove one particle
  /*! remove one particle from #p (see ARC::chainlist.remove())
     @param [in] i: particle index in #p needs to be removed
     @param [in] option: position update option: 
                 - true: shift last particle to current position (defaulted);
                 - false: shift all right particle to left by one
  */
  void removeP(const int i, bool option=true) { 
      p.remove(i,option); 
      F_Pmod=true; 
  }

  //! Data Dumping to file function
  /*! Dump chain data into file (binary format) using fwrite. 
      The first variable in the dumping data is particle number #num
      The second is the particle chain list index array #list
      The third is one dimensional array data storing #t, #B, #w, #X, #V generated by using backup().
      The forth is center of mass particle data
      The fifth is one dimensional array storing the masses of particles
      The sixth is slowdown parameters
      @param [in] pout: std::FILE pointer for dumping the data
   */
  void dump(std::FILE *pout) {
      // number of particles
      fwrite(&num,sizeof(int),1,pout);
      // chain list
      fwrite(list,sizeof(int),num,pout);
      // data
      const int dsize=6*num-3;
      Float *dtemp=new Float[dsize+1];
      dtemp[dsize]=dsize;
      backup(dtemp);
      fwrite(dtemp,sizeof(Float),dsize+1,pout);
      // Pot, Ekin
      fwrite(&Ekin,sizeof(Float),1,pout);
      fwrite(&Pot,sizeof(Float),1,pout);
      // center-of-mass particle
      particle::writeBinary(pout);
      //Float cmass=particle::getMass();
      //fwrite(&cmass,sizeof(Float),1,pout);
      //fwrite(particle::getPos(),sizeof(Float),3,pout);
      //fwrite(particle::getVel(),sizeof(Float),3,pout);
      
      // member particle
      for(int i=0; i<num; i++) p[i].writeBinary(pout);
      // mass of particles
      //Float *pmass=new Float[num];
      //p.getMassAll(pmass);
      //fwrite(pmass,sizeof(Float),num,pout);
      
      // interaction_parameters
      // if (Int_pars) fwrite(Int_pars,sizeof(int_par),1,pout);
      slowdown.dump(pout);

//      std::cerr<<"Chain dumping:\nNumber of stars ="<<num<<std::endl;
//      std::cerr<<"Chain list index =";
//      for (int i=0; i<num;i++) std::cerr<<list[i]<<" ";
//      std::cerr<<"\nChain parameters t, B, w, X[][], V[][]= ";
//      for (int i=0; i<dsize+1;i++) std::cerr<<dtemp[i]<<" ";
//      std::cerr<<"\nMass of particles =";
//      for (int i=0; i<num;i++) std::cerr<<pmass[i]<<" ";
//      std::cerr<<std::endl;
      
      delete[] dtemp;
      //delete[] pmass;
  }

  // Dump all data including chain, chainpars and necessary information (chain.smpars and #ds)
  /*
    call chain.dump, chain.dump
   */
  

  //! Data reading from file function
  /*! Read chain data from binary file generated by dump(). 
      The first variable in the dumping data is particle number #num
      The second is the particle chain list index array #list
      The third is one dimensional array data storing #t, #B, #w, #X, #V generated by using backup().
      The forth is center of mass particle data
      The fifth is one dimensional array storing the masses of particles

      Notice the particle list will allocate new memory to store the particle data. The particle data are in the center-of-mass frame
      @param [in] filename: std::FILE pointer for reading the data
   */
  void read(std::FILE *pin) {
      // number of particles
      int n;
      int rn;
      rn = fread(&n,sizeof(int),1,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read number of particles!\n";
        abort();
      }
      if(n>nmax||p.getN()>0) {
        clear();
        allocate(n);
      }
      num = n;

      // chain list
      rn = fread(list,sizeof(int),n,pin);
      if(rn<n) {
        std::cerr<<"Error: reading chain list fails, required number of data is "<<n<<", only got "<<rn<<"!\n";
        abort();
      }
      
      // data
      const int dsize=6*n-3;
      Float *dtemp=new Float[dsize+1];
      rn = fread(dtemp,sizeof(Float),dsize+1,pin);
      if(rn<=dsize) {
        std::cerr<<"Error: reading chain data fails, required data size is "<<dsize+1<<", only got "<<rn<<"!\n";
        abort();
      }
      
      restore(dtemp);

      //Pot, Ekin
      rn = fread(&Ekin,sizeof(Float),1,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read Ekin!\n";
        abort();
      }
      rn = fread(&Pot,sizeof(Float),1,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read Pot!\n";
        abort();
      }
      
      
      // center-of-mass
      particle::readBinary(pin);
      //Float cmass,cx[3],cv[3];
      //rn = fread(&cmass,sizeof(Float),1,pin);
      //if(rn<1) {
      //  std::cerr<<"Error: cannot read center-of-mass!\n";
      //  abort();
      //}
      //rn = fread(cx,sizeof(Float),3,pin);
      //if(rn<3) {
      //  std::cerr<<"Error: reading position of center-of-mass particle fails, required reading 3 values, only got "<<rn<<"!\n";
      //  abort();
      //}
      //rn = fread(cv,sizeof(Float),3,pin);
      //if(rn<3) {
      //  std::cerr<<"Error: reading velocity of center-of-mass particle fails, required reading 3 values, only got "<<rn<<"!\n";
      //  abort();
      //}
      // 
      //particle::setMass(cmass);
      //particle::setPos(cx[0],cx[1],cx[2]);
      //particle::setVel(cv[0],cv[1],cv[2]);
      
      //Float *pmass=new Float[n];
      //rn = fread(pmass,sizeof(Float),n,pin);
      //if(rn<n) {
      //  std::cerr<<"Error: reading particle masses fails, required number of data is "<<n<<", only got "<<rn<<"!\n";
      //  abort();
      //}

      //Int_pars=new int_par;
      //rn = fread(Int_pars,sizeof(int_par),1,pin);
      //if (rn<1) {
      //  delete Int_pars;
      //  Int_pars=NULL;
      //}
                 
      // member particles
      p.allocate(n);
      for (int i=0;i<n;i++) {
          p.add(particle());
          p[i].readBinary(pin);
      }

      slowdown.read(pin);

//      std::cerr<<"Chain loading:\nNumber of stars ="<<n<<std::endl;
//      std::cerr<<"Chain list index =";
//      for (int i=0; i<n;i++) std::cerr<<list[i]<<" ";
//      std::cerr<<"\nChain parameters t, B, w, X[][], V[][]= ";
//      for (int i=0; i<dsize+1;i++) std::cerr<<dtemp[i]<<" ";
//      std::cerr<<"\nMass of particles =";
//      for (int i=0; i<n;i++) std::cerr<<pmass[i]<<" ";
//      std::cerr<<std::endl;

      F_read=true;
      
      delete[] dtemp;
      //delete[] pmass;

//#ifdef ARC_PROFILE
//      profile.initstep(pars.exp_itermax+1);
//#endif
  }

  //! Read particle data from file
  /*! Read particle data using fread (binary format). Number of particles should be first variable, then the data of particles (the data structure should be consistent with particle class).
      Notice if read is used, the particle data memory is allocated inside chainlist of chain.
    @param [in] filename: file to read the data
   */
  void readP(const char* filename) {
    p.read(filename);
  }

  //! resolve particles and copy back to original particle address (if particles are not linked by linkP())
  /*! 
      If particles are in center-of-mass frame, return to original frame and then copyback to original address
   */
  void resolve() {
    if(F_Porigin!=1) center_shift_inverse();
    p.copyback();
  }
  
  ////! Allocate memory for perturber list
  ///*! Allocate memory for perturber particle list with maximum number of \a n
  //   @param [in] n: maximum number of perturbers
  // */
  //void initPext(const int n) {
  //  if (pext.getNmax()) {
  //    std::cerr<<"Error: Perturber list is already initialized!\n";
  //    abort();
  //  }
  //  pext.init(n);
  //}
  ////! Add one perturber particle
  ///*! Add one perturber particle (address pointer) into #pext
  //   @param [in] a: perturber particle
  // */
  //void addPext(particle &a) { pext.add(a);}
  // 
  ////! Add one chain as a perturber particle
  ///*! Add one chain as a perturber particle (address pointer) into #pext
  //   @param [in] a: the chain perturber 
  // */
  //void addPext(chain<particle,int_par> &a) { pext.add(a);}
  // 
  ////! Add a list of perturber particles
  ///*! Add a list of perturber particles (address pointer) into #pext
  //   @param [in] n: number of particles need to be added
  //   @param [in] a: array of perturbers (address)
  // */
  //void addPext(const int n, particle a[]) { pext.add(n,a); }
  // 
  ////! Remove one perturber 
  ///*! Remove one perturber from #pext
  //   @param [in] i: perturber index in #pext needs to be removed
  //   @param [in] option: position update option: 
  //               - true: shift last particle to current position (defaulted);
  //               - false: shift all right particle to left by one
  //*/
  //void removePext(const int i, bool option=true) { pext.remove(i,option); }
  
  //! Indicator of changes in particle list #p
  /*! Reture true if particle list is modifed, in this case, the chain may need to be initialized again
      \return true: Particle list is modified
  */
  bool isPmod() const { return F_Pmod; }

  //! Indicator of particle frame
  /*! Return true if particles positions and velocities in #p are in original frame
     \return: - 1: In the original frame
              - 2: Only positions are in the original frame
              - 0: In the center-of-mass frame
  */
  int isPorigin() const { return F_Porigin; }

  //! Get particle i (const reference)
  /*!
    \return: particle i (const reference)
   */
  particle& getP(const int i) const {
    return p[i];
  }

  //! Get Particle original address (const reference)
  /*!
    @param[in] i: particle index in p
    \return: particle original address of i
   */
  particle** getPAdr() const {
    return p.getPAdr();
  }

  ////! Get pertuber particle i (const reference)
  ///*!
  //  \return: pertuber particle i (const reference)
  // */
  //const particle& getPext(const int i) const {
  //  return pext[i];
  //}

  //! Get current number of chain members
  /*!
    Notice this is not always the number of particles.
    If AddP(), removeP() are used, init() should be called first to get consistent number.
    \return: number of chain members
  */
  int getN() const{
    return num;
  }

  //! Get maximum particle number
  /*! \return Current maximum particle number that can be stored in particle address list #p
   */
  int getNmax() const {
    return nmax;
  }

  //! Initial chain
  /*! Initialize chain based on particle list #p. After this function, positions and velocities of particles in #p will be shifted to their center-of-mass frame. \n
    Chain order list #list, relative position #X, velocity #V are calculated
   */
  void initChain() {
      // update number indicator
      // update_num(p.getN());
      num = p.getN();
    
      // Generate chain link list
      generate_list();

      // set center-of-mass
      if (F_Porigin==1) {
          center_shift_init();
          F_Porigin=0;
      }
      else {
          std::cerr<<"Error: particles are not in original frame!\n";
          abort();
      }

      // set member relative position and velocity
      calc_XV();
  }

  //! Initialization of intergration parameters and slowdown time and perturbation
  /*! Initial system energy #Pt and initial time transformation parameter #w are calculated.
    The particle modification indicator (isPmod()) will be set to false.
    Slowdown perturbation force and real time is initailized
    @param [in] time: current time of particle system (real time)
    @param [in] pars: chainpars controller
    @param [in] int_pars: extra parameters used in f
    @param [in] pert: perturber particle array
    @param [in] pertf: perturrber force array for prediction
    @param [in] npert: number of perturbers
    \return fail_flag: true: fail
  */
  template<class pertparticle_, class pertforce_, class extpar_>
  void initSys(const Float time, 
               const chainpars &pars, 
               extpar_* int_pars,
               pertparticle_* pert = NULL, 
               pertforce_* pertf = NULL, 
               const int npert = 0) {
    pair_AW<particle,extpar_> f = reinterpret_cast<pair_AW<particle,extpar_>>(pars.pp_AW);
    ext_Acc<particle,pertparticle_,pertforce_,extpar_> fpert = reinterpret_cast<ext_Acc<particle,pertparticle_,pertforce_,extpar_>>(pars.ext_A);
    if(std::type_index(typeid(f))!=*pars.pp_AW_type) {
        std::cerr<<"Error: acceleration function type not matched the data type\n";
        abort();
    }
    if(fpert!=NULL) {
        if(std::type_index(typeid(fpert))!=*pars.ext_A_type) {
            std::cerr<<"Error: perturber function type not matched the data type\n";
            abort();
        }
    }
    if(F_read) {
        // initialization
        // resolve_XV();

        F_Porigin = 0;
        F_Pmod = false;

        // initial pf
        initial_pf();

        // get potential and transformation parameter
        // calc_rAPW(f, int_pars);

        // update slow-down
        // updateSlowDownFpert();
 
        // kinetic energy
        // calc_Ekin();
        
    }
    else {
    
        // Initial intgrt value t (avoid confusion of real time when slowdown is used)
        t = Float(0.0);

        slowdown.t_real = time;

        // initial pf
        initial_pf();
        
        // perturber force
        if (fpert!=NULL) {
            // should not do this, since center of mass position is changed during integration
            //// get original position first, update p.x (dependence: X, particle::x)
            //// center_shift_inverse_X();

            // Update perturber force pf (dependence: pext, original-frame p.x, p.getMass())
#ifdef ARC_WARN
            int stat=fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
            if (stat!=0&&info==NULL) {
                info = new chaininfo(num);
                info->status = stat;
            }
#else
            fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
#endif
            //// reset position to center-of-mass frame, update p.x 
            //// center_shift_X();
        }

        // set relative distance matrix, acceleration, potential and transformation parameter, notice force will be recalculated later.
        calc_rAPW(f, int_pars);

        // update slow-down
        initialSlowDownFpert();
 
        // kinetic energy
        calc_Ekin();

        // initial time step parameter
        Pt = -Pot - Ekin;
        w = W;

        // set F_Pmod to false
        F_Pmod = false;
    }

    /*#ifdef DEBUG
//    for (int i=0;i<num;i++) {
//      std::cout<<i<<" m"<<std::setw(WIDTH)<<p[i].getMass()<<std::setw(WIDTH)<<"x";
//      for (int k=0;k<3;k++) std::cout<<std::setw(WIDTH)<<p[i].pos[k];
//      std::cout<<std::setw(WIDTH)<<"v ";
//      for (int k=0;k<3;k++) std::cout<<std::setw(WIDTH)<<p[i].vel[k];
//      std::cout<<std::endl;
//    }
//    std::cout<<std::setw(WIDTH)<<"kin:"<<std::setw(WIDTH)<<0.5*(p[0].getMass()*p[1].getMass()/(p[0].getMass()+p[1].getMass()))*(V[0][0]*V[0][0]+V[0][1]*V[0][1]+V[0][2]*V[0][2])<<std::endl;
    std::cerr<<std::setw(WIDTH)<<t;
    for (int i=0;i<num-1;i++) {
      for (int k=0;k<3;k++) std::cerr<<std::setw(WIDTH)<<X[i][k];
      for (int k=0;k<3;k++) std::cerr<<std::setw(WIDTH)<<V[i][k];
    }
    std::cerr<<std::setw(WIDTH)<<Ekin<<std::setw(WIDTH)<<Pot<<std::setw(WIDTH)<<Pt+Ekin-Pot<<std::setw(WIDTH)<<Pt<<std::setw(WIDTH)<<w<<std::endl;
    #endif*/
#ifdef ARC_PROFILE
    if(!profile.stepcount) {
        if(pars.exp_sequence>0) profile.initstep(pars.exp_itermax+1);
        else profile.initstep(1);
    }
#endif
    
  }

  ////! link interaction parameter class
  ///*! link interaction parameter clsss to chain for the acceleration calculation functions
  //  @param[in] par: int_par type interaction parameter class. The address will be stored (not copy)
  // */
  //void link_int_par(int_par &par) {
  //  if (F_read) {
  //    std::cerr<<"Error: interaction parameter variable Int_pars is already set during load(), linking is forbidden!\n";
  //    abort();
  //  }
  //  else Int_pars = &par;
  //}

  ////! Get interaction parameter
  ///*! \return The reference of Int_pars, can be modified
  // */
  //int_par &get_int_par() {
  //  return *Int_pars;
  //}

  //! Inversed center-of-mass frame shift for particles
  /*! Shift the position and velocities of particles in #p from center-of-mass frame to original frame
      Notice the center-of-mass position and velocity use values from #cm
  */
  void center_shift_inverse() {
    if (F_Porigin==1) {
      std::cerr<<"Warning: particles are already in original frame!\n";
    }
    else {
      const Float *rc = particle::getPos();
      const Float *vc = particle::getVel();
      for (int i=0;i<num;i++) {
        if (F_Porigin==0) {
          const Float *ri = p[i].getPos();
          p[i].setPos(ri[0] + rc[0],
                      ri[1] + rc[1],
                      ri[2] + rc[2]);
        }
        const Float *vi = p[i].getVel();
        p[i].setVel(vi[0] + vc[0],
                    vi[1] + vc[1],
                    vi[2] + vc[2]);
      }
      F_Porigin = 1;
    }
  }

  //! Center-of-mass frame shift for particles
  /*! Shift positions and velocities of particles in #p from their original frame to their center-of-mass frame\n
      Notice the center-of-mass position and velocity use values from #cm
  */
  void center_shift() {
    if (F_Porigin>0) {
      const Float *rc = particle::getPos();
      const Float *vc = particle::getVel();
      for (int i=0;i<num;i++) {
        const Float *ri = p[i].getPos();
        p[i].setPos(ri[0] - rc[0],
                    ri[1] - rc[1],
                    ri[2] - rc[2]);
        if (F_Porigin==1) {
          const Float *vi = p[i].getVel();
          p[i].setVel(vi[0] - vc[0],
                      vi[1] - vc[1],
                      vi[2] - vc[2]);
        }
      }
      F_Porigin = 0;
    }
    else {
      std::cerr<<"Warning: particles are already in the center-of-mass frame!\n";
    }      
  }

  //! Backup chain basic data (#t, #Pt, #w, #X, #V)
  /*! Backup chain data to one dimensional array. 
      - #t : current physical time
      - #Pt : current time momentum (system binding energy)
      - #w : current time transformation parameter
      - #X : current relative position array
      - #V : current relative velocite array
     @param [out] db: backup array (size should be 6*#num-3) where #num is the total number of particles in #p
   */
  void backup(Float* db) {
#ifdef ARC_DEBUG
    const int dsize=6*num-3;
    if ((int)db[dsize]!=dsize) {
      std::cerr<<"Error: data array size ("<<(int)db[dsize]<<") for backup is not matched, should be ("<<dsize<<")!\n";
      abort();
    }
#endif
    const int ndata=3*(num-1);
    db[0] = t;
    db[1] = Pt;
    db[2] = w;
    std::memcpy(&db[3], X[0], ndata*sizeof(Float));
    std::memcpy(&db[3+ndata], V[0], ndata*sizeof(Float));
  }

  //! Backup chain basic and integrated data (#t, #Pt, #w, #Ekin, #Pot, #X, #V)
  /*! Backup chain data to one dimensional array. 
      - #t : current physical time
      - #Pt : current time momentum (system binding energy)
      - #w : current time transformation parameter
      - #Ekin: kinetic energy
      - #Pot: Potential energy
      - #X : current relative position array
      - #V : current relative velocite array
     @param [out] db: backup array (size should be 6*#num-3) where #num is the total number of particles in #p
   */
  void backupInt(Float* db) {
#ifdef ARC_DEBUG
    const int dsize=6*num-1;
    if ((int)db[dsize]!=dsize) {
      std::cerr<<"Error: data array size ("<<(int)db[dsize]<<") for backup is not matched, should be ("<<dsize<<")!\n";
      abort();
    }
#endif
    const int ndata=3*(num-1);
    db[0] = t;
    db[1] = Pt;
    db[2] = w;
    db[3] = Ekin;
    db[4] = Pot;
    std::memcpy(&db[5], X[0], ndata*sizeof(Float));
    std::memcpy(&db[5+ndata], V[0], ndata*sizeof(Float));
  }
     
  //! Restore chain data (#t, #Pt, #w, #X, #V)
  /*! Restore integration data from one dimensional array, the order of data should be #t, #Pt, #w, #X[#num][3], #V[#num][3]
      - #t : current physical time
      - #Pt : current time momentum (system binding energy)
      - #w : current time transformation parameter
      - #X : current relative position array
      - #V : current relative velocite array
     @param [in] db: one dimensional array that storing chain data (array size should be 6*#num-3) where #num is the total number of particles in #p
   */
  void restore(Float* db) {
#ifdef ARC_DEBUG
    const int dsize=6*num-3;
    if ((int)db[dsize]!=dsize) {
      std::cerr<<"Error: data array size ("<<(int)db[dsize]<<") for restore is not matched, should be ("<<dsize<<")!\n";
      abort();
    }
#endif
    const int ndata=3*(num-1);
    t = db[0];
    Pt = db[1];
    w = db[2];
    std::memcpy(X[0], &db[3], ndata*sizeof(Float));
    std::memcpy(V[0], &db[3+ndata], ndata*sizeof(Float));
  }

  //! Restore chain basic and integrated data (#t, #Pt, #w, #Ekin, #Pot, #X, #V)
  /*! Restore integration data from one dimensional array, the order of data should be #t, #Pt, #w, #X[#num][3], #V[#num][3]
      - #t : current physical time
      - #Pt : current time momentum (system binding energy)
      - #w : current time transformation parameter
      - #Ekin: kinetic energy
      - #Pot: Potential energy
      - #X : current relative position array
      - #V : current relative velocite array
     @param [in] db: one dimensional array that storing chain data (array size should be 6*#num-3) where #num is the total number of particles in #p
   */
  void restoreInt(Float* db) {
#ifdef ARC_DEBUG
    const int dsize=6*num-1;
    if ((int)db[dsize]!=dsize) {
      std::cerr<<"Error: data array size ("<<(int)db[dsize]<<") for restore is not matched, should be ("<<dsize<<")!\n";
      abort();
    }
#endif
    const int ndata=3*(num-1);
    t = db[0];
    Pt = db[1];
    w = db[2];
    Ekin = db[3];
    Pot = db[4];
    std::memcpy(X[0], &db[5], ndata*sizeof(Float));
    std::memcpy(V[0], &db[5+ndata], ndata*sizeof(Float));
  }

  //! Leapfrog integrator
  /*! Integration with Leapfrog method. \n
      The positions and velocities of particles in #p will be integrated in the center-of-mass frame
      @param [in] s: Integration step size
      @param [in] n: number of sub-steps needed to be divided. Integration step size is (\a s/\a n) and do \a n times as: X(s/2n)V(s/n)X(s/n)V(s/n)..X(s/2n)
      @param [in] pars: chainpars controller
      @param [in] int_pars: extra parameters used in f or fpert.
      @param [in] pert: perturber particle array
      @param [in] pertf: perturrber force array for prediction
      @param [in] npert: number of perturbers
      @param [in] check_flag: 2: check link every step; 1: check link at then end; 0 :no check
      @param [in] dpoly: two dimensional array for storing 0 to \a (ndmax-*)'th order central difference of physical time #t at \a s/2 as a function of \a s, array size should be [ndmax][1]
      @param [in] ndmax: dpoly array size and the maximum difference is ndmax-*
  */             
//               recur_flag: flag to determine whether to resolve sub-chain particles for force calculations. notice this require the sub-chain to be integrated to current physical time. Thus is this a recursion call (tree-recusion-integration)
//               upforce: void (const particle * p, const particle *pext, Float3* force). function to calculate force based on p and pext, return to force
  template<class pertparticle_, class pertforce_, class extpar_>
  void Leapfrog_step_forward(const Float s, 
                             const int n, 
                             chainpars &pars,
                             extpar_ *int_pars = NULL,
                             pertparticle_* pert = NULL, 
                             pertforce_* pertf = NULL, 
                             const int npert = 0,
                             int check_flag = 1, 
                             Float dpoly[][2] = NULL, 
                             const int ndmax = 0) {
#ifdef ARC_PROFILE
    profile.t_lf -= get_wtime();
#endif

    pair_AW<particle,extpar_> f = reinterpret_cast<pair_AW<particle_,extpar_>>(pars.pp_AW);
    ext_Acc<particle,pertparticle_,pertforce_,extpar_> fpert = reinterpret_cast<ext_Acc<particle,pertparticle_,pertforce_,extpar_>>(pars.ext_A);

#ifdef ARC_DEBUG
    // Error check
    if (n<=0) {
      std::cerr<<"Error: step number shound be positive, current number is "<<n<<std::endl;
      abort();
    }
    /*    if (s<0) {
      std::cerr<<"Error: step size should be positive, current value is "<<s<<std::endl;
      abort();*/
    if (s==0) {
      std::cerr<<"Error: step size should not be zero!\n"<<std::endl;
      abort();
    }
    if (F_Porigin) {
      std::cerr<<"Error: particles are not in the center-of-mass frame, the integration can be dangerous!"<<std::endl;
      abort();
    }
    if (F_Pmod) {
      std::cerr<<"Error: particles are modified, initialization required!"<<std::endl;
      abort();
    }

    if(fpert!=NULL) {
        if(std::type_index(typeid(fpert))!=*pars.ext_A_type) {
            std::cerr<<"Error: perturber function type not matched the data type\n";
            abort();
        }
    }
#endif

    Float ds = s/Float(n);
    Float3 ave_v[nmax];              // average velocity
//    bool fpf = false;                // perturber force indicator
    //const int np = pext.getN();
    //if (np>0||pars->ext_A!=NULL) fpf = true;

    // for polynomial coefficient calculation first point
    // middle difference (first array is used to store the f_1/2)
    if(dpoly!=NULL) {
#ifdef ARC_PROFILE
      profile.t_dense -= get_wtime();
#endif
      if (pars.exp_sequence==3) {
        dpoly[1][0]=calc_dt_V(1.0,pars);  //storage left edge derivate
        mid_diff_calc(&dpoly[4],ndmax-4,0,n,pars);
      }
      else {
        dpoly[0][0]=calc_dt_V(1.0,pars);  //storage left edge derivate
        edge_diff_calc(&dpoly[1],ndmax-1,0,n,pars);
      }
#ifdef ARC_PROFILE
      profile.t_dense += get_wtime();
#endif
    }
                                               
    
    // integration loop
    for (int i=0;i<n;i++) {
      // half step forward for t (dependence: Ekin, Pt, w)
      Float dt = calc_dt_X(ds*0.5,pars);
#ifdef ARC_WARN
      if (info!=NULL) {
        info->inti = i;
        info->subds = ds;
        info->Ekin = Ekin;
        info->Pot = Pot;
        info->W = W;
        check_flag=0;
        break;
      }
#endif
      // step_forward time
      t += dt;

      // update real time
      slowdown.updateRealTime(dt);
      
      // recursive integration-----------------------------------------------//
      /*
#ifdef ARC_PROFILE
      profile.t_lf += get_wtime();
#endif
      // check sub-chain 
      const int nct = p.getNchain();
      if (recur_flag&&nct>0) {
        // get chainlist table
        chain<particle,int_par> **clist = new chain<particle,int_par>*[nct];

        // looping to link sub-chains
        const int nt = p.getN();

        int k = 0;
        for (int j=0; j<nt; j++) {
          if (p.isChain(j)) {
            clist[k] = p.getSub(j);
            k++;
          }
        }

        // call tree recursion integration
        for (int j=0; j<k; j++) {
          // recursively call leapfrog until reaching the deepest branch.
          if (clist[j]->p.getNchain()>0)  clist[j]->Leapfrog_step_forward(ds, n, t, force, 1, true);
          else clist[j]->extrapolation_integration(ds, t, force);
        }
      }
#ifdef ARC_PROFILE
      profile.t_lf -= get_wtime();
#endif
      */
      //---------------------------------------------------------------------//

      // half step forward X (dependence: V, dt)
      step_forward_X(dt);

      // resolve X to p.v (dependence: X, particle::getMass())
      resolve_X();
      
      
      // perturber force
      if (fpert!=NULL) {
        // should not do this, since center of mass position is changed during integration
        //// get original position first, update p.x (dependence: X, particle::x)
        //// center_shift_inverse_X();

        // Update perturber force pf (dependence: pext, original-frame p.x, p.getMass())
#ifdef ARC_WARN
          int stat=fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
          if (stat!=0&&info==NULL) {
              info = new chaininfo(num);
              info->status = stat;
              info->inti = i;
              info->subds = ds;
              check_flag=0;
              break;
          }
#else
        fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
#endif
        //// reset position to center-of-mass frame, update p.x 
        //// center_shift_X();
      }

      // Update rjk, A, Pot, dWdr, W for half X (dependence: pf, force, p.m, p.x, X)
      calc_rAPW(f, int_pars);
#ifdef ARC_WARN
      if (info!=NULL) {
        info->inti = i;
        info->subds = ds;
        check_flag=0;
        break;
      }
#endif
      // update slow-down
      updateSlowDownFpert();

      // Update chain list order if necessary, update list, X, V (dependence: p.x, X, V)
      if (check_flag==2) {
          if (num>2) update_link();
          else {
              // update rmax and rmin
              rmin2_ = X[0][0]*X[0][0] + X[0][1]*X[0][1] + X[0][2]*X[0][2];
              rmax2_ = rmin2_;
          }
      }

      // Get time step dt(V) (dependence: Pot, W)
      Float dvt = calc_dt_V(ds,pars);

      // Step forward V (dependence: dt(V), V, A)
      step_forward_V(dvt);
      
      // Get averaged velocity, update p.x, p.v, ave_v (dependence: X, V)
      resolve_XV(ave_v);

      // forward Pt and w (dependence: dt(V), ave_v, p.m, p.v, dWdr, pf)
      const bool dw_calc_flag = pars.beta>0;
      step_forward_Ptw(dvt,ave_v,dw_calc_flag);

      // Calcuale Kinetic energy (dependence: p.m, p.v)
      calc_Ekin();

      // step forward for t (dependence: Ekin, Pt, w)
      dt = calc_dt_X(ds*0.5,pars);

#ifdef ARC_WARN
      // accident check
      if (info!=NULL) {
        info->inti = i;
        info->subds = ds;
        info->Ekin = Ekin;
        info->Pot = Pot;
        info->W = W;
        check_flag=0;
        break;
      }
#endif

      t += dt;

      // update real time
      slowdown.updateRealTime(dt);

      // step forward for X (dependence: X, V)
      step_forward_X(dt);
      
      // for interpolation polynomial coefficient (difference)
      // middle difference (first array is used to store the f_1/2)
      if(dpoly!=NULL) {
#ifdef ARC_PROFILE
        profile.t_dense -= get_wtime();
#endif
        if(pars.exp_sequence==3) {
          if (i==n/2-1) {
            dpoly[0][0]=t; // y
            dpoly[3][0]=2.0*dt/ds; // f(x)
//#ifdef ARC_DEEP_DEBUG
//            std::cerr<<"Mid time = "<<t<<", n="<<n<<"; i="<<i+1<<std::endl;
//#endif
          }
          mid_diff_calc(&dpoly[4],ndmax-4,i+1,n,pars);
        }
        // edge difference
        else edge_diff_calc(&dpoly[1],ndmax-1,i+1,n,pars);
#ifdef ARC_PROFILE
        profile.t_dense += get_wtime();
#endif
      }
    }

    // resolve X at last, update p.x (dependence: X)
    resolve_X();

    // Update rjk, A, Pot, dWdr, W (notice A will be incorrect since pf is not updated)
    calc_rAPW(f, int_pars);

    // update slow-down
    updateSlowDownFpert();

#ifdef ARC_PROFILE
    profile.t_dense -= get_wtime();
#endif
    if(dpoly!=NULL) {
      if(pars.exp_sequence==3) dpoly[2][0]=calc_dt_V(1.0,pars);
      else dpoly[0][1]=calc_dt_V(1.0,pars);
    }
#ifdef ARC_PROFILE
    profile.t_dense += get_wtime();
#endif

//#ifdef ARC_DEEP_DEBUG
//    std::cerr<<"Ending time = "<<t<<", n="<<n<<std::endl;
//#endif
//#ifdef ARC_DEEP_DEBUG
//    std::cerr<<std::setw(WIDTH)<<t;
//    //      for (int i=0;i<num;i++) 
//    //        for (int k=0;k<3;k++) std::cerr<<p[i].pos[k]<<" ";
//    for (int i=0;i<num-1;i++) {
//      for (int k=0;k<3;k++) std::cerr<<std::setw(WIDTH)<<X[i][k];
//      for (int k=0;k<3;k++) std::cerr<<std::setw(WIDTH)<<V[i][k];
//    }
//    std::cerr<<std::setw(WIDTH)<<Ekin<<std::setw(WIDTH)<<Pot<<std::setw(WIDTH)<<Pt+Ekin-Pot<<std::setw(WIDTH)<<Pt<<std::setw(WIDTH)<<w<<std::endl;
//#endif
      
    // update chain list order and calculate potential
    if (check_flag==1) {
        if(num>2)  update_link();
        else {
            // update rmax and rmin
            rmin2_ = X[0][0]*X[0][0] + X[0][1]*X[0][1] + X[0][2]*X[0][2];
            rmax2_ = rmin2_;
        }
    }

    // clear memory
    //delete[] ave_v;
#ifdef ARC_PROFILE
    profile.t_lf += get_wtime();
#endif
  }

  //! Extrapolation integration
  /*! Use extrapolation method to get highly accurate integration based on Leapfrog_step_forward().
    The auto-determination of extrapolation orders based on the accuracy requirement is used. 
     @param [in] ds: integration step size
     @param [in] pars: chainpars controller
     @param [in] toff_real: ending physical time
                      - if value is negative, it means integration will be done with fixed step size \a ds
                      - if value is positive and after step \a ds, the ending physical time is larger than \a toff, the interpolation of physical time #t (dense output) will be done instead of integration. In this case, the data are kept as initial values. Instead, the returning value is the ds modification factor (negative value), which can be used to modified current \a ds and redo the integration by calling this function again with new ds to approach ending physical time of \a toff. Notice if the required time sychronization criterion (set in chainpars.setEXP()) is small (<phase and energy error criterion), several iteration may be needed to get the physical time below this criterion.
     @param [in] int_pars: extra parameters used in f or fpert.
     @param [in] pert: perturber particle array. Notice the c.m. of chain also need to be updated during perturbation force calculation, thus it is convinient to set first perturber as chain c.m.
     @param [in] pertf: perturrber force array for prediction. For the same purpose as above, the first force can be set as chain c.m. force
     @param [in] npert: number of perturbers
     @param [in] err_ignore: if true, force integration and ignore error criterion (default false)
     \return factor
            - if factor is positive, it is optimized step size modification factor for next step (\a ds *= factor)
            - if factor is negative and \a toff>0; the -factor is used for calculate new ds' = -factor * \a ds. Thus this function should be called again with new step size ds' and the new result should have ending physical time close to \a toff.
            - if factor is zero, maximum extrapolation sequence index (accuracy order/iteration times) is fixed (\ref ARC::chainpars) and err_ingore is false, it means the error criterion cannot be satisfied with current maximum sequence index. In this case no integration is done and the data are kept as initial values. User should reduce the integration step and re-call this function.
   */
  template<class pertparticle_, class pertforce_, class extpar_>
  Float extrapolation_integration(const Float ds, 
                                   chainpars &pars,
                                   const Float toff_real = -1, 
                                   extpar_ *int_pars = NULL,
                                   pertparticle_* pert = NULL, 
                                   pertforce_* pertf = NULL, 
                                   const int npert = 0,
                                   const bool err_ignore=false) {
#ifdef ARC_PROFILE
    profile.t_ep -= get_wtime();
#endif
    pair_AW<particle,extpar_> f = reinterpret_cast<pair_AW<particle,extpar_>>(pars.pp_AW);
#ifdef ARC_DEBUG
    if(std::type_index(typeid(f))!=*pars.pp_AW_type) {
        std::cerr<<"Error: acceleration function type not matched the data type\n";
        abort();
    }
#endif
    // clear info if exist
    if (info){
      delete info;
      info=NULL;
    }
    
    // slowdown time
    const Float toff_sd = (toff_real-slowdown.t_real)/slowdown.kappa + t;

    // get parameters
    const Float error = pars.exp_error;
    const int itermax = pars.exp_itermax;
    const int method = pars.exp_method;
    const int sq = pars.exp_sequence;
    const int *step = pars.step;
    
    // array size indicator for relative position and velocity
    const int nrel = num-1;
    // data storage size (extra one is used to show the size of the array for safety check)
    const int dsize = 6*nrel+3;
    const int darray = dsize+1;
    // edge difference array size;
    // const int psize=dsize*2;
    
    // for storage
    // for convenient, the data are storaged in one array with size (2*nrel+1)*3, which represents t, Pt, w, X[3][nrel], V[3][nrel]
    Float d0[darray],dtemp[darray];
    Float dn[itermax][darray];
    Float* dnptr[itermax];
    d0[dsize] = (Float)dsize;    // label for safety check
    dtemp[dsize] = (Float)dsize; // label for safety check
    for (int i=0; i<itermax; i++) {
      dn[i][dsize] = (Float)dsize; // label for safety check
      dnptr[i] = dn[i];
    }
    Float Ekin0,Pot0,T_real0;

    // for dense output polynomial
#ifdef ARC_PROFILE
    profile.t_dense -= get_wtime();
#endif
    bool ip_flag = true;  // interpolation coefficient calculation flag
    const int ndiffmax=pars.den_intpmax*2+5; // maximum number of difference and f(x) values
    Float2 pd[itermax][ndiffmax]; // central difference, first [] indicate different accuracy level , second [] for storage f(x) and difference, third[](Float2) store individal (t), middle one value, edge two value
    int ndmax[itermax];   // maximum difference order
    int pnn;      // data size
    if(sq==3) pnn = 1;     // middle difference case
    else pnn = 2;         // edge two points case
#ifdef ARC_PROFILE
    profile.t_dense += get_wtime();
#endif

    // for error check
    Float3 CX={};
    Float3 CXN;
    Float cxerr=error+1.0;
    Float eerr=error+1.0;
    Float cxerr0=cxerr+1.0;
    Float eerr0=eerr+1.0;
    // error for step estimation
    Float werrmax=std::numeric_limits<Float>::max();

    // backup initial values (t, Pt, w, X, V, Ekin)
    backup(d0);
    Ekin0 = Ekin;
    Pot0 = Pot;
    T_real0 = slowdown.t_real;

    // new step
    Float dsn = 1.0;
    
    int intcount = 0;  // iteration counter
    int itercount = 0; // iteration efforts count
    
    bool reset_flag=false;  // reseting flag
    
    while (true) {
      if (intcount>0) {
        // reset the initial data (t, Pt, w, X, V, Ekin)
        restore(d0);
        Ekin = Ekin0;
        Pot = Pot0;
        slowdown.t_real = T_real0;
        // reset velocity to get correct w
        resolve_V();
      }

      // Dense output
#ifdef ARC_PROFILE
      profile.t_dense -= get_wtime();
#endif
      if (ip_flag) {
        // middle difference case: difference order from 1 to 2*intcount+2 (2*kappa-2; kappa=intcount+1), first one is used to storage f(x), 2,3 are used for edge f'(x) at (0, ds)
        if(sq==3) ndmax[intcount] = 2*std::min(intcount,pars.den_intpmax)+5;
        // edge difference case: difference order from 1 to intcount+1, first store f(x)
        else ndmax[intcount] = std::min(intcount,pars.den_intpmax)+2;
        
        // pd[][*]: * storage f(x) and difference
        // pd[intcount] = new Float*[ndmax[intcount]];
        // pd[][][*]: * storage data (t, Pt, w, X, V)
        // for (int j=0;j<ndmax[intcount];j++) pd[intcount][j] = new Float[pnn];
      }
      //else pd[intcount] = NULL;
#ifdef ARC_PROFILE
      profile.t_dense += get_wtime();
#endif

      // intergration
      Leapfrog_step_forward<pertparticle_,pertforce_,extpar_>(ds,step[intcount],pars,int_pars,pert,pertf,npert,0,pd[intcount],ndmax[intcount]);

#ifdef ARC_WARN
      // accident detection
      if (info!=NULL) {
        reset_flag = true;
        info->intcount = intcount+1;
        info->ds = ds;
        break;
      }
#endif

      // increase iteration counter
      itercount += step[intcount];
      
      if (intcount == 0) {
        // storage the results to [n]
        backup(dn[intcount]);

        // relative position vector between first and last particle for phase error check
        for (int i=0;i<3;i++) CX[i] = 0.0;
        for (int i=0;i<num-1;i++) {
          CX[0] += X[i][0];
          CX[1] += X[i][1];
          CX[2] += X[i][2];
        }
      }
      else {
        // storage the results to [temp]
        backup(dtemp);
        
        // iteration
        // Using Polynomial method
        if (method==1) EP::polynomial_extrapolation(dnptr,dtemp,step,dsize,intcount);
        // Using Rational interpolation method
        else EP::rational_extrapolation(dnptr,dtemp,step,dsize,intcount);

        // set final results back to chain array
        restore(dn[intcount]);
 
        // resolve particle
        resolve_XV();
        // recalculate the energy
        calc_Ekin();
        // force, potential and W
        calc_rAPW(f,int_pars);

        // update slow-down
        updateSlowDownFpert();

        // phase error calculation
        for (int i=0;i<3;i++) CXN[i] = 0.0;
        for (int i=0;i<nrel;i++) {
          CXN[0] += X[i][0];
          CXN[1] += X[i][1];
          CXN[2] += X[i][2];
        }
        Float RCXN2 = CXN[0]*CXN[0] + CXN[1]*CXN[1] + CXN[2]*CXN[2];
      
        Float dcx1 = CXN[0] - CX[0];
        Float dcx2 = CXN[1] - CX[1];
        Float dcx3 = CXN[2] - CX[2];
        cxerr0 = cxerr;
        cxerr = sqrt((dcx1*dcx1 + dcx2*dcx2 + dcx3*dcx3)/RCXN2);
        eerr0 = eerr;
        eerr = abs((Ekin+Pot+Pt-Ekin0-Pot0-d0[1])/Pt);
        //        std::cerr<<"Ekin="<<Ekin<<" Pot="<<Pot<<" Pt="<<Pt<<" Ekin0="<<Ekin0<<" Pot0="<<Pot0<<" Pt0="<<d0[1]<<" eerr="<<eerr<<std::endl;
        std::memcpy(CX,CXN,3*sizeof(Float));

        if (pars.auto_step==1) {
          // get error estimation
          Float ermax=std::min(EP::extrapolation_error(dnptr,dsize,intcount),std::min(eerr,cxerr));
          Float dsfactor = EP::H_opt_factor(ermax,error,intcount+1);
          Float werrn = ((Float)itercount+num)/ dsfactor;
          if (ermax>0&&werrn<werrmax) {
            werrmax = werrn;
            dsn = dsfactor;
#ifdef ARC_DEEP_DEBUG
            std::cerr<<"ERR factor update: sequence="<<step[intcount]<<"; modify factor="<<dsfactor<<"; ermax="<<ermax<<"; eerr="<<eerr<<"; cxerr="<<cxerr<<"; ds="<<ds<<std::endl;
#endif
          }
        }
      }
      
      intcount++;

#ifdef ARC_DEEP_DEBUG
      std::cerr<<std::setprecision(6)<<"Iter.= "<<intcount<<" Dep.= "<<step[intcount]<<" P-err.= "<<cxerr;
      std::cerr<<" E-err.="<<Ekin+Pot+Pt-Ekin0-Pot0-d0[1]<<" Pt ="<<std::setprecision(12)<<Pt<<std::endl;
#endif 

      // error and convergency check
      if (!pars.exp_fix_iter) {
        if (cxerr <= 0.5*error && eerr <= 0.1*error) break;
        
        if (cxerr>=cxerr0 || cxerr==0 || eerr >= eerr0) {
          if (cxerr < error && eerr < error) break;
          else if (intcount > std::min(10,itermax)){
//#ifdef ARC_WARN            
//            std::cerr<<"Warning: extrapolation cannot converge anymore, energy error - current: "<<eerr<<"  previous: "<<eerr0<<"   , phase error - current: "<<cxerr<<"  previous: "<<cxerr0<<", try to change the error criterion (notice energy error is cumulative value)\n";
//#endif
            // in the case of serious energy error, quit the simulation and dump the data
           if (eerr*std::min(Float(1.0),abs(Pt))>100.0*error) {
              reset_flag=true;
              info=new chaininfo(num);
              info->status=1;
              info->intcount = intcount;
              info->ds = ds;
              info->perr  = cxerr;
              info->perr0 = cxerr0;
              info->eerr  = eerr;
              info->eerr0 = eerr0;
            }
            break;
          }
        }
      }

      // iteration limit check
      if (intcount == itermax) {
        if((cxerr < error && eerr < error) || err_ignore) break;
        reset_flag=true;
        info=new chaininfo(num);
        info->status=2;
        info->intcount = intcount;
        info->ds = ds;
        info->perr  = cxerr;
        info->perr0 = cxerr0;
        info->eerr  = eerr;
        info->eerr0 = eerr0;
        break;
      }
    }

    // for dense output
    if (!reset_flag&&ip_flag&&toff_sd>0&&toff_sd<t&&abs((toff_sd-t)/(t-d0[0]))>pars.dterr) {
#ifdef ARC_PROFILE
      profile.t_dense -= get_wtime();
#endif

#ifdef ARC_DEEP_DEBUG
      std::cerr<<"ds="<<ds<<" step[0]="<<step[0]<<" terr="<<toff_sd-t<<" t="<<t<<" toff_sd="<<toff_sd<<std::endl;
#endif
      // calculate derivates from differences
      for (int i=0; i<intcount; i++) {
        // first difference pointer
         
        int dpsize;
        Float2* pdptr;
        Float h;
        if(sq==3) {
          // middle difference case first element is f(x)
          pdptr=&pd[i][4];
          // differece order number should be reduced by one
          dpsize = ndmax[i]-4;
          // step size
          h = 2*ds/(Float)step[i];
        }
        else {
          pdptr=&pd[i][1];
          dpsize = ndmax[i]-1;
          h = ds/(Float)step[i];
        }

        diff_dev_calc(pdptr,h,dpsize,pnn); 
      }        
      
      // extrapolation table
      Float2 pn[intcount];
      Float* pnptr[intcount];
      for (int i=0; i<intcount; i++) pnptr[i] = pn[i];
      
      //for (int i=0; i<intcount; i++) pn[i] = new Float[pnn];

      // starting accuracy index
      int istart=0;
      
      // from low difference order (1) to high difference order (ndmax) 
      for (int i=0; i<ndmax[intcount-1]; i++) {

        // find correct istart;
        if (i>=ndmax[istart]) istart++;

        if (istart>=intcount) break;
        
        // storage result of first order accuracy
        std::memcpy(pn[0],pd[istart][i],pnn*sizeof(Float));
#ifdef ARC_DEEP_DEBUG
        std::cerr<<"Poly calc order="<<istart<<" step("<<istart<<")="<<step[istart]<<" t X11^("<<i+1<<")_"<<istart<<"="<<pd[istart][i][0]<<"\t"<<pd[istart][i][3]<<std::endl;
#endif
        // extrapolation to higher order accuracy
        for (int j=istart+1; j<intcount; j++) {
#ifdef ARC_DEEP_DEBUG
          std::cerr<<"Poly calc order="<<j<<" step("<<istart<<")="<<step[istart]<<" t X11^("<<i+1<<")_"<<j<<"="<<pd[j][i][0]<<"\t"<<pd[j][i][3]<<std::endl;
#endif
          if (method==1) EP::polynomial_extrapolation(pnptr,pd[j][i],&step[istart],pnn,j-istart);
          if (method==2) EP::rational_extrapolation(pnptr,pd[j][i],&step[istart],pnn,j-istart);
#ifdef ARC_DEEP_DEBUG
          std::cerr<<"Poly extra order="<<j-istart<<" step("<<istart<<")="<<step[istart]<<" t X11^("<<i+1<<")_"<<j<<"="<<pd[j][i][0]<<"\t"<<pd[j][i][3]<<std::endl;
#endif
        }
#ifdef ARC_DEEP_DEBUG
          std::cerr<<"Final result t X11^("<<i+1<<")="<<pd[intcount-1][i][0]<<"\t"<<pd[intcount-1][i][3]<<std::endl;
#endif
      }

      // number of points
      const int npoints=2;
      // store position s

      Float xpoint[npoints];
      // maximum difference level

      int nlev[npoints];
      // polynomial

      const int Ncoff=(sq==3? (ndmax[intcount-1]) : (2*ndmax[intcount-1]+2));
      Float pcoff[Ncoff];

      // data point
      Float* fpoint[npoints];

      // final derivate starting pointer
      const int dfptr_size = (sq==3? ndmax[intcount-1]-3: ndmax[intcount-1]);
      Float* dfptr[dfptr_size][2];
      Float** dfpptr[dfptr_size];
      for(int i=0; i<dfptr_size; i++) dfpptr[i]=dfptr[i];
      // checking flag
      bool no_intp_flag=false;

      // expected ds, t      
      Float dsm,tpre;             

      // for middle difference case (1 point)
      if(sq==3) {

        //pcoff = new Float[ndmax[intcount-1]];

        // dfptr=new Float**[ndmax[intcount-1]+1];
        
        // first choose the half side for interpolation
        const Float tmid = pd[intcount-1][0][0];  // middle time
        if (toff_sd>tmid) {
        
          xpoint[0]=0.5*ds;
          xpoint[1]=ds;

          nlev[0] = ndmax[intcount-1]-2;
          nlev[1] = 2;

          fpoint[0] = pd[intcount-1][0];
          fpoint[1] = dn[intcount-1];

          //dfptr_size = nlev[0]-1;
          for (int i=0;i<dfptr_size;i++) {
            // dfptr[i]=new Float*[2];
            dfptr[i][0]=pd[intcount-1][i+3];
            dfptr[i][1]=NULL;
          }
          dfptr[0][1]=pd[intcount-1][2]; //right edge
        }
        else if (toff_sd==tmid) {
          dsm = 0.5;
          no_intp_flag = true;
        }
        else {
          xpoint[0]=0.0;
          xpoint[1]=0.5*ds;

          nlev[0] = 2;
          nlev[1] = ndmax[intcount-1]-2;
          
          fpoint[0] = d0;
          fpoint[1] = pd[intcount-1][0];

          //dfptr_size = nlev[1]-1;
          for (int i=0;i<dfptr_size;i++) {
            //  dfptr[i]=new Float*[2];
            dfptr[i][0]=NULL;
            dfptr[i][1]=pd[intcount-1][i+3];
          }
          dfptr[0][0]=pd[intcount-1][1]; //left edge 
        }
      }
      // for edge difference case (2 points)
      else {
        //npoints=2;

        xpoint[0]=0.0;
        xpoint[1]=ds;

        nlev[0] = ndmax[intcount-1]+1;
        nlev[1] = nlev[0];
        
        // store f(x)
        fpoint[0] = d0;
        fpoint[1] = dn[intcount-1];

        // \sum nlev = 2*intcount+2;
        //pcoff= new Float[2*ndmax[intcount-1]+2];

        //dfptr_size = nlev[0]-1;
        //dfptr=new Float**[dfptr_size];
        for (int i=0;i<nlev[0]-1;i++) {
          // dfptr[i] = new Float*[2];
          dfptr[i][0] = pd[intcount-1][i];
          dfptr[i][1] = &pd[intcount-1][i][1];
        }
      }

      if (!no_intp_flag) {
    
        // Hermite interpolation
        Float* pcoffptr = pcoff;
        
        EP::Hermite_interpolation_coefficients(&pcoffptr,xpoint,fpoint,dfpptr,1,npoints,nlev);

#ifdef ARC_DEEP_DEBUG
        std::cerr<<"PCOFF: ";
        for (int i=0;i<ndmax[intcount-1]+2;i++) std::cerr<<" "<<pcoff[i];
        std::cerr<<std::endl;
#endif

        // Iteration to get correct physical time position
        Float dsi[2]   = {xpoint[0],xpoint[1]};    // edges for iteration
        Float tsi[2]   = {fpoint[0][0],fpoint[1][0]}; // edges values
        const Float dterr = pars.dterr*(t-d0[0]);
        const Float dterr3 = 1000*dterr;  // 1000 * time error criterion

        bool rf_method=false;
        int find_root_count=0;
        do {
          if (rf_method) {
            dsm = (dsi[0]*(tsi[1]-toff_sd)-dsi[1]*(tsi[0]-toff_sd))/(tsi[1]-tsi[0]);  // Use regula falsi method to find accurate ds
          }
          else {
            dsm = (dsi[0]+dsi[1])*0.5;      // Use bisection method to get approximate region
          }

          Float* pcoffptr = pcoff;
          EP::Hermite_interpolation_polynomial(dsm,&tpre,&pcoffptr,xpoint,1,npoints,nlev);
          // safety check
          if (tpre > tsi[1]||tpre < tsi[0]) {
            reset_flag=true;
            info = new chaininfo(num);
            info->status = 5;
            info->intcount = intcount;
            info->ds = ds;
            info->terr = tpre-toff_sd;
            break;
          }
        
          if (tpre > toff_sd) {
            if (dsi[1]==dsm) break;
            dsi[1] = dsm;
            tsi[1] = tpre;
          }
          else {
            if (dsi[0]==dsm) break;
            dsi[0] = dsm;
            tsi[0] = tpre;
          }
#ifdef ARC_DEEP_DEBUG
          std::cerr<<std::setprecision(15)<<"Find root: dsm="<<dsm<<"; t="<<tpre<<"; error="<<tpre-toff_sd<<"; ds="<<ds<<std::endl;
#endif
          if(abs(tpre-toff_sd)<dterr3) rf_method=true;
          find_root_count++;
          if (find_root_count>100) {
            reset_flag=true;
            info = new chaininfo(num);
            info->status = 3;
            info->intcount = intcount;
            info->ds = ds;
            info->terr = tpre-toff_sd;
            break;
          }
        } while (abs(tpre-toff_sd)>0.1*dterr);

        // Get the interpolation result
        //EP::Hermite_interpolation_polynomial(dsm,dtemp,&pcoff,xpoint,1,npoints,nlev);

        // update time factor
        dsn = -(dsm/ds);

      }
      // avoid energy issue
      //if(cxerr < error && eerr < error) dsn = 0.5*ds;
      
      // update the results
      //restore(dtemp);

      // reset the data to original
      restore(d0);
      Ekin = Ekin0;
      Pot  = Pot0;
      slowdown.t_real = T_real0;
      // reset velocity to get correct w
      resolve_V();

//#ifdef ARC_DEEP_DEBUG
//      /*std::cerr<<"Getting: ds= "<<dsm;
//      for (int i=0;i<dsize;i++) std::cerr<<" "<<dtemp[i];
//      std::cerr<<std::endl;
//      
//      EP::Hermite_interpolation_polynomial(0,dtemp,&pcoff,xpoint,dsize,2,nlev);
//      std::cerr<<"Starting:";
//      for (int i=0;i<dsize;i++) std::cerr<<" "<<dtemp[i];
//      std::cerr<<std::endl;
// 
//      std::cerr<<"Ending:";
//      EP::Hermite_interpolation_polynomial(ds,dtemp,&pcoff,xpoint,dsize,2,nlev);
//      for (int i=0;i<dsize;i++) std::cerr<<" "<<dtemp[i];
//      std::cerr<<std::endl;
//      */
//      for (int i=0; i<=1000; i++) {
//        dsm = ds/1000*i;
//        EP::Hermite_interpolation_polynomial(dsm,&tpre,&pcoff,xpoint,1,npoints,nlev);
////        EP::Hermite_interpolation_polynomial(dsm,dtemp,pcoff,xpoint,dsize,npoints,nlev);
////        // update the results
////        restore(dtemp);
////        // resolve particle
////        resolve_XV();
////        // recalculate the energy
////        calc_Ekin();
////        // force, potential and W
////        calc_rAPW(force);
////      
//        std::cerr<<"Loop: "<<dsm;
//        std::cerr<<" "<<std::setprecision(15)<<tpre;
////        std::cerr<<" "<<(Ekin-Pot+Pt)/Pt;
////        for (int i=0;i<dsize;i++) std::cerr<<std::setprecision(10)<<" "<<dtemp[i];
//        std::cerr<<std::endl;
//      }
//      
//#endif

      // clear memory
      //for (int i=0; i<intcount; i++) delete[] pn[i];
      //if (dfptr_size>0) 
      //    for (int i=0; i<dfptr_size; i++) delete dfptr[i];
      //delete[] dfptr;
      //delete[] pcoff;
      //delete[] xpoint;
      //delete[] nlev;
      //delete[] fpoint;

#ifdef ARC_PROFILE
      profile.t_dense += get_wtime();
#endif
    }
    else if (!reset_flag){
      // auto-step
#ifdef ARC_PROFILE
      profile.t_newdt -= get_wtime();
#endif
      if (pars.auto_step==1)
        dsn = std::min(std::max(dsn,pars.auto_step_fac_min),pars.auto_step_fac_max);
      else if (pars.auto_step==2) {
        dsn = calc_next_step_XVA(pars)/ds;
        dsn = std::min(std::max(dsn,pars.auto_step_fac_min),pars.auto_step_fac_max);
      }
      else if (pars.auto_step==3) {
        if      (intcount>pars.auto_step_iter_max) dsn = pars.auto_step_fac_min;
        else if (intcount<pars.auto_step_iter_min) dsn = pars.auto_step_fac_max;
        else    dsn = 1.0;
      }
      else if (pars.auto_step==4) {
        dsn = calc_next_step_custom(pars,int_pars)/ds;
      }
#ifdef ARC_PROFILE
      profile.t_newdt += get_wtime();
#endif
      // update chain link order
      if(num>2) update_link();
      else {
          // update rmax and rmin
          rmin2_ = X[0][0]*X[0][0] + X[0][1]*X[0][1] + X[0][2]*X[0][2];
          rmax2_ = rmin2_;
      }
    }

    if(reset_flag) {
      dsn = 0.0;
      // reset the initial data (t, Pt, w, X, V, Ekin)
      backup(info->data);
      restore(d0);
      Ekin = Ekin0;
      Pot  = Pot0;
      slowdown.t_real = T_real0;
      // reset velocity to get correct w
      resolve_V();
    }

    // clear memory
    //for (int i=0; i<itermax; i++) delete[] dn[i];
    // 
    //if (ip_flag) {
    //  for (int i=0; i<intcount; i++) {
    //    if (pd[i]!=NULL) {
    //      for (int j=0; j<ndmax[i]; j++) delete[] pd[i][j];
    //      delete[] pd[i];
    //    }
    //  }
    //}

#ifdef ARC_PROFILE
    profile.t_ep += get_wtime();
    if(!profile.stepcount) profile.initstep(pars.exp_itermax+1);
    profile.stepcount[intcount+1]++;
    profile.itercount +=itercount;
#endif

    return dsn;
  }


  //! high order symplectic integrator
  /*! Integration with high order symplectic integrator
      The positions and velocities of particles in #p will be integrated in the center-of-mass frame
      @param [in] s: Integration step size
      @param [in] pars: chainpars controller
      @param [out] timetable: an array that store the time after each drift
      @param [in] int_pars: extra parameters used in f or fpert.
      @param [in] pert: perturber particle array
      @param [in] pertf: perturrber force array for prediction
      @param [in] npert: number of perturbers
      @param [in] check_flag: check link at end (default: true)
      @param [in] update_sd_flag: whether update slowdown factor
  */
  template<class pertparticle_, class pertforce_, class extpar_>
  void Symplectic_integration(const Float s, 
                              const chainpars &pars,
                              Float* timetable,
                              extpar_ *int_pars = NULL,
                              pertparticle_* pert = NULL, 
                              pertforce_* pertf = NULL, 
                              const int npert = 0,
                              bool check_flag = true,
                              bool update_sd_flag = true) {
#ifdef ARC_PROFILE
    profile.t_sym -= get_wtime();
#endif

#ifdef FAPP_PROFILE
    fapp_start("sym",1,1);
#endif

    pair_AW<particle,extpar_> f = reinterpret_cast<pair_AW<particle_,extpar_>>(pars.pp_AW);
    ext_Acc<particle,pertparticle_,pertforce_,extpar_> fpert = reinterpret_cast<ext_Acc<particle,pertparticle_,pertforce_,extpar_>>(pars.ext_A);

#ifdef ARC_DEBUG
    // Error check
    /*    if (s<0) {
      std::cerr<<"Error: step size should be positive, current value is "<<s<<std::endl;
      abort();*/
    if (pars.sym_n==0) {
      std::cerr<<"Error: symplectic integrator order is not initialized!\n";
      abort();
    }
    if (s==0) {
      std::cerr<<"Error: step size should not be zero!\n"<<std::endl;
      abort();
    }
    if (F_Porigin) {
      std::cerr<<"Error: particles are not in the center-of-mass frame, the integration can be dangerous!"<<std::endl;
      abort();
    }
    if (F_Pmod) {
      std::cerr<<"Error: particles are modified, initialization required!"<<std::endl;
      abort();
    }

    if(fpert!=NULL) {
        if(std::type_index(typeid(fpert))!=*pars.ext_A_type) {
            std::cerr<<"Error: perturber function type not matched the data type\n";
            abort();
        }
    }
#endif
    Float3 ave_v[nmax];              // average velocity

    const bool dw_calc_flag = pars.beta>0;

    // integration with cofficients table
    for (int i=0;i<pars.sym_k;i++) {
 
     // Drift t (dependence: Ekin, Pt, w)
      Float dt = calc_dt_X(pars.sym_coff[i][0]*s,pars);

#ifdef ARC_WARN
      if (info!=NULL) {
        info->inti = i;
        info->subds = s;
        info->Ekin = Ekin;
        info->Pot = Pot;
        info->W = W;
        break;
      }
#endif
      // step_forward time
      t += dt;

      // update real time
      slowdown.updateRealTime(dt);

      // store current time
      if(timetable!=NULL) timetable[i] = slowdown.t_real;

      // Drift X (dependence: V, dt)
      step_forward_X(dt);

      // resolve X to p.x (dependence: X, particle::getMass())
      resolve_X();
      
      // perturber force
      if (fpert!=NULL) {
        // Update perturber force pf (dependence: pext, original-frame p.x, p.getMass())
#ifdef ARC_WARN
          int stat = fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
          if (stat!=0&&info!=NULL) {
              info = new chaininfo(num);
              info->status = stat;
              info->inti = i;
              break;
          }
#else
          fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
#endif
      }

      // Update rjk, A, Pot, dWdr, W for half X (dependence: pf, force, p.m, p.x, X)
      calc_rAPW(f, int_pars);
      if (info!=NULL) {
        info->inti = i;
        break;
      }

      // update slow-down
      updateSlowDownFpert();

      // Kick time step dt(V) (dependence: Pot, W)
      Float dvt = calc_dt_V(pars.sym_coff[i][1]*s,pars);

      // Step forward V (dependence: dt(V), V, A)
      step_forward_V(dvt);
      
      // Get averaged velocity, update p.x, p.v, ave_v (dependence: X, V)
      resolve_XV(ave_v);

      // forward Pt and w (dependence: dt(V), ave_v, p.m, p.v, dWdr, pf)
      step_forward_Ptw(dvt,ave_v,dw_calc_flag);

      // Calcuale Kinetic energy (dependence: p.m, p.v)
      calc_Ekin();

    }
    

    // update chain list order
    if(check_flag) {
        if(num>2)  update_link();
        else {
            // update rmax and rmin
            rmin2_ = X[0][0]*X[0][0] + X[0][1]*X[0][1] + X[0][2]*X[0][2];
            rmax2_ = rmin2_;
        }
    }

    if(update_sd_flag) slowdown.updateKappaMin();

#ifdef FAPP_PROFILE
    fapp_stop("sym",1,1);
#endif

#ifdef ARC_PROFILE
    profile.t_sym += get_wtime();
#endif
  }

#ifdef ARC_OPT_SYM2
  //! high order symplectic integrator for two-body
  /*! Integration with high order symplectic integrator
      The positions and velocities of particles in #p will be integrated in the center-of-mass frame
      @param [in] s: Integration step size
      @param [in] pars: chainpars controller
      @param [out] timetable: an array that store the time after each drift
      @param [in] m2_mt: m2/(m1+m2)
      @param [in] m1_m2: m1/m2
      @param [in] int_pars: extra parameters used in f or fpert.
      @param [in] pert: perturber particle array
      @param [in] pertf: perturrber force array for prediction
      @param [in] npert: number of perturbers
      @param [in] update_sdk_flag: whether to update slowdown factor
  */
  template<class pertparticle_, class pertforce_, class extpar_>
  void Symplectic_integration_two(const Float s, 
                                  const chainpars &pars,
                                  Float* timetable,
                                  const Float m2_mt,
                                  const Float m1_m2_1,
                                  extpar_ *int_pars = NULL,
                                  pertparticle_* pert = NULL, 
                                  pertforce_* pertf = NULL, 
                                  const int npert = 0,
                                  const bool update_sd_flag=true) {
#ifdef ARC_PROFILE
    profile.t_sym -= get_wtime();
#endif
    pair_AW<particle,extpar_> f = reinterpret_cast<pair_AW<particle_,extpar_>>(pars.pp_AW);
    ext_Acc<particle,pertparticle_,pertforce_,extpar_> fpert = reinterpret_cast<ext_Acc<particle,pertparticle_,pertforce_,extpar_>>(pars.ext_A);

#ifdef ARC_DEBUG
    // Error check
    /*    if (s<0) {
      std::cerr<<"Error: step size should be positive, current value is "<<s<<std::endl;
      abort();*/
    if (pars.sym_n==0) {
      std::cerr<<"Error: symplectic integrator order is not initialized!\n";
      abort();
    }
    if (s==0) {
      std::cerr<<"Error: step size should not be zero!\n"<<std::endl;
      abort();
    }
    if (F_Porigin) {
      std::cerr<<"Error: particles are not in the center-of-mass frame, the integration can be dangerous!"<<std::endl;
      abort();
    }
    if (F_Pmod) {
      std::cerr<<"Error: particles are modified, initialization required!"<<std::endl;
      abort();
    }

    if(fpert!=NULL) {
        if(std::type_index(typeid(fpert))!=*pars.ext_A_type) {
            std::cerr<<"Error: perturber function type not matched the data type\n";
            abort();
        }
    }

    if(timetable==NULL) {
        std::cerr<<"Error: timetable point is NULL!"<<std::endl;
        abort();
    }
#endif
    const bool dw_calc_flag = pars.beta>0;
    const bool fpert_flag = fpert!=NULL;
//    const Float m1_m2_1 = -m1_m2 - 1.0;
    Float fp2 = Float(0.0);
    Float At2 = std::numeric_limits<Float>::max();
    Float fratio = Float(0.0);
    //Float fc2 = At2, fcp2 = 0.0;
    const Float m1 = p[0].getMass();
    const Float m2 = p[1].getMass();

    // integration with cofficients table
    for (int i=0;i<pars.sym_k;i++) {

        // Drift t (dependence: Ekin, Pt, w)
        Float dt = pars.sym_coff[i][0]*s / (pars.alpha * (Ekin + Pt) + pars.beta * w + pars.gamma);

//#ifdef ARC_WARN        
//        if (abs(dt) < pars.dtmin && info==NULL) {
//            info=new chaininfo(num);
//            info->status = 4;
//            info->subdt = dt;
//            info->inti = i;
//            info->subds = s;
//            info->Ekin = Ekin;
//            info->Pot = Pot;
//            info->W = W;
//            break;
//        }
//#endif

        // step_forward time
        t += dt;

        // update real time
        slowdown.updateRealTime(dt);

        // store current time
        timetable[i] = slowdown.t_real;
        
        // step forward relative X
        X[0][0] += dt * V[0][0];
        X[0][1] += dt * V[0][1];
        X[0][2] += dt * V[0][2];
        
        Float3 x1,x2;
        x1[0] = - m2_mt*X[0][0];
        x1[1] = - m2_mt*X[0][1];
        x1[2] = - m2_mt*X[0][2];
        
        x2[0] = x1[0] + X[0][0];
        x2[1] = x1[1] + X[0][1];
        x2[2] = x1[2] + X[0][2];

        p[0].setPos(x1[0],x1[1],x1[2]);
        p[1].setPos(x2[0],x2[1],x2[2]);

#ifdef ARC_WARN
        if(fpert_flag) {
            int stat=fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
            if (stat!=0&&info==NULL) {
                info = new chaininfo(num);
                info->status = stat;
                info->i1 = 0;
                info->i2 = 1;
                info->inti = i;
                break;
            }
        }
#else
        if(fpert_flag) fpert(pf, slowdown.t_real, p.getData(), p.getN(), pert, pertf, npert, int_pars);
#endif        
        Float3 At,dWt;
        Float Pott,Wt;

#ifdef ARC_WARN
        int stat=f(At, Pott, dWt, Wt, X[0], p[0], p[1], int_pars);
        if (stat!=0&&info==NULL) {
          info = new chaininfo(num);
          info->status = stat;
          info->i1 = 0;
          info->i2 = 1;
          info->inti = i;
          break;
        }
#else
        f(At, Pott, dWt, Wt, X[0], p[0], p[1], int_pars);
#endif

        Pot = Pott;
        W = Wt;

        // Kick time step dt(V) (dependence: Pot, W)
        Float dvt = pars.sym_coff[i][1]*s / (pars.gamma - pars.alpha * Pot + pars.beta * W);

        // Acceleration and kick
        Float3 dpf;
        dpf[0] = pf[1][0]-pf[0][0]; 
        dpf[1] = pf[1][1]-pf[0][1];
        dpf[2] = pf[1][2]-pf[0][2];

        At[0] *= m1_m2_1;
        At[1] *= m1_m2_1;
        At[2] *= m1_m2_1;

        V[0][0] += dvt * (At[0] + slowdown.kappa*dpf[0]); 
        V[0][1] += dvt * (At[1] + slowdown.kappa*dpf[1]); 
        V[0][2] += dvt * (At[2] + slowdown.kappa*dpf[2]); 

        Float3 v1,v2,ave_v[2];
        v1[0] = - m2_mt*V[0][0];
        v1[1] = - m2_mt*V[0][1];
        v1[2] = - m2_mt*V[0][2];
        
        v2[0] = v1[0] + V[0][0];
        v2[1] = v1[1] + V[0][1];
        v2[2] = v1[2] + V[0][2];

        const Float* pv1=p[0].getVel();
        const Float* pv2=p[1].getVel();
        
        ave_v[0][0] = 0.5* (v1[0] + pv1[0]);
        ave_v[0][1] = 0.5* (v1[1] + pv1[1]);
        ave_v[0][2] = 0.5* (v1[2] + pv1[2]);

        ave_v[1][0] = 0.5* (v2[0] + pv2[0]);
        ave_v[1][1] = 0.5* (v2[1] + pv2[1]);
        ave_v[1][2] = 0.5* (v2[2] + pv2[2]);

        p[0].setVel(v1[0],v1[1],v1[2]);
        p[1].setVel(v2[0],v2[1],v2[2]);

        Pt -= dvt * (m1 * (ave_v[0][0] * slowdown.kappa*pf[0][0] +
                           ave_v[0][1] * slowdown.kappa*pf[0][1] +
                           ave_v[0][2] * slowdown.kappa*pf[0][2])
                   + m2 * (ave_v[1][0] * slowdown.kappa*pf[1][0] +
                           ave_v[1][1] * slowdown.kappa*pf[1][1] +
                           ave_v[1][2] * slowdown.kappa*pf[1][2]));
        
        if(dw_calc_flag) w += dvt*(ave_v[0][0] * dWt[0] +
                                   ave_v[0][1] * dWt[1] +
                                   ave_v[0][2] * dWt[2] -
                                   ave_v[1][0] * dWt[0] - 
                                   ave_v[1][1] * dWt[1] - 
                                   ave_v[1][2] * dWt[2]);
        
        Ekin = 0.5 * (m1 * (v1[0]*v1[0]+v1[1]*v1[1]+v1[2]*v1[2])
                     +m2 * (v2[0]*v2[0]+v2[1]*v2[1]+v2[2]*v2[2]));
        
        At2 = At[0]*At[0] + At[1]*At[1] + At[2]*At[2];
        fp2 = dpf[0]*dpf[0] + dpf[1]*dpf[1] + dpf[2]*dpf[2];
        fratio = std::max(fratio, fp2/At2);
        //fc2 = std::min(fc2, pf[0][0]*pf[0][0] + pf[0][1]*pf[0][1] + pf[0][2]*pf[0][2]);
        //Float dfc1 = m1*pf[0][0]-m2*pf[1][0];
        //Float dfc2 = m1*pf[0][1]-m2*pf[1][1];
        //Float dfc3 = m1*pf[0][2]-m2*pf[1][2];
        //fcp2= std::max(fcp2, dfc1*dfc1+dfc2*dfc2+dfc3*dfc3);
    }

    // resolve X
    Float3 x1,x2;
    x1[0] = - m2_mt*X[0][0];
    x1[1] = - m2_mt*X[0][1];
    x1[2] = - m2_mt*X[0][2];
        
    x2[0] = x1[0] + X[0][0];
    x2[1] = x1[1] + X[0][1];
    x2[2] = x1[2] + X[0][2];

    p[0].setPos(x1[0],x1[1],x1[2]);
    p[1].setPos(x2[0],x2[1],x2[2]);

    // update slowdown factor
    slowdown.updateFRatioSqRange(fratio);
    if(update_sd_flag) {
        slowdown.updateKappaMin();
    }

#ifdef ARC_PROFILE
    profile.t_sym += get_wtime();
#endif

  }  
    
#endif

  //! high order symplectic integrator with time synchronization
  /*! Integration with high order symplectic integrator
      The positions and velocities of particles in #p will be integrated in the center-of-mass frame
      @param [in] s: Integration step size
      @param [in] pars: chainpars controller
      @param [in] tend_real: real time to finish the integration
      @param [in] int_pars: extra parameters used in f or fpert.
      @param [in] pert: perturber particle array
      @param [in] pertf: perturrber force array for prediction
      @param [in] npert: number of perturbers
      @param [in] fix_step_flag: 0: adjust step for both initial and integration based on energy error; 1: adjust initial step but fix after that; 2: fix step and use input step s
      \return step counter
  */
  template<class pertparticle_, class pertforce_, class extpar_>
  int Symplectic_integration_tsyn(const Float s, 
                                  const chainpars &pars,
                                  const Float tend_real,
                                  extpar_ *int_pars = NULL,
                                  pertparticle_* pert = NULL, 
                                  pertforce_* pertf = NULL, 
                                  const int npert = 0,
                                  const int fix_step_flag = 0,
                                  const int max_nstep=100000) {

      // real full time step
      const Float dt_real_full = tend_real-slowdown.t_real;
      // time synchronization
      const Float terr_real = dt_real_full*pars.dterr;

      const int dsize  = 6*(num-1)+5;
      const int darray = dsize+1; // backup data array size;
      Float bk[darray]; // for backup chain data
      Float bksd[5]; // for slowdown backup
#ifdef ARC_DEBUG_DUMP
      Float bk0[darray], bksd0[5]; // for backup
      particle p0[num];
#endif
#ifdef ARC_DEBUG
      bk[dsize] = (Float)dsize;  // label for safety check
#ifdef ARC_DEBUG_DUMP
      bk0[dsize] = (Float)dsize;
#endif      
#endif
      const int symk = pars.sym_k;
      Float timetable[symk]; // for storing time information
      
      //const Float t0_real = slowdown.t_real; // backup initial time
      Float ds[2] = {s,s}; // step with a buffer
      Float dsbk = s;  //backup step size
      int dsk=0;
      int nsub=-1; // substep number
      int nsubcount=0; // counter for tsyn steps
      int stepcount = 0;
      int stepcount_tsyn=0; // after time end
      bool bk_flag=true; // flag for backup or restore
      const Float eerr_min = pars.sym_An*0.5*pars.exp_error;
      //Float eerr_pre=eerr_min; // energy error of previous step
      bool tend_flag=false; // go to ending step
      bool update_kappa_flag=false;

#ifdef ARC_DEBUG
      Float Ekin_check = 0.0;
      for (int i=0; i<num; i++) {
          Ekin_check += 0.5*p[i].getMass()*(p[i].getVel()[0]*p[i].getVel()[0]+p[i].getVel()[1]*p[i].getVel()[1]+p[i].getVel()[2]*p[i].getVel()[2]);
      }
      assert(abs(Ekin_check-Ekin)<1e-10);
#endif

#ifdef ARC_DEBUG_DUMP
      backupInt(bk0);
      slowdown.backup(bksd0);
      for (int i=0; i<num; i++) p0[i]=p[i];
#endif
      
#ifdef ARC_OPT_SYM2
      const Float pm1 = p[0].getMass();
      const Float pm2 = p[1].getMass();
      const Float m2_mt = pm2/(pm1+pm2);
      const Float m1_m2_1 = -pm1/pm2-1.0;
#endif
      while(true) {
          // backup /restore data
          if(bk_flag) {
              backupInt(bk);
              slowdown.backup(bksd);
          }
          else {
              restoreInt(bk);
              slowdown.restore(bksd);
              resolve_V();
          }
          
          // integrate one step

          // integrated dt;
          Float dt_real = slowdown.t_real;

#ifdef ARC_OPT_SYM2
          if(num==2) Symplectic_integration_two(ds[dsk], pars, timetable, m2_mt, m1_m2_1, int_pars, pert, pertf, npert, update_kappa_flag);
          else Symplectic_integration(ds[dsk], pars, timetable, int_pars, pert, pertf, npert, false, update_kappa_flag);
#else 
          Symplectic_integration(ds[dsk], pars, timetable, int_pars, pert, pertf, npert, false, update_kappa_flag);
#endif

          // real step size
          dt_real = slowdown.t_real - dt_real;

          stepcount++;

#ifdef ARC_WARN
          if(stepcount>=max_nstep&&stepcount%max_nstep==0) {
              std::cerr<<"Warning: stepcount is signficiant "<<stepcount<<std::endl;
              std::cerr<<"Time: "<<t<<" Tend(real): "<<tend_real<<" dterr(real): "<<(slowdown.t_real-tend_real)/dt_real_full
                       <<" ds_used: "<<ds[dsk]<<" ds_next: "<<ds[1-dsk]<<" error: "<<abs((Ekin+Pot+Pt-bk[3]-bk[4]-bk[1])/Pt)<<std::endl;
              print(std::cerr);
          }
          if (info!=NULL) return -stepcount;
#endif
          
          if(stepcount_tsyn>max_nstep-10) {
              std::cerr<<"Error! stepcount after time synchronization >"<<max_nstep<<std::endl;
              std::cerr<<"Time: "<<t<<" Tend(real): "<<tend_real<<" dterr(real): "<<(slowdown.t_real-tend_real)/dt_real_full
                       <<" ds_used: "<<ds[dsk]<<" ds_next: "<<ds[1-dsk]<<" error: "<<abs((Ekin+Pot+Pt-bk[3]-bk[4]-bk[1])/Pt)<<std::endl;
              std::cerr<<"Tend_flag: "<<tend_flag<<" dt(real): "<<dt_real<<" subcount "<<nsubcount<<" dsbk: "<<dsbk<<std::endl;
#ifdef ARC_DEBUG_DUMP
              if(stepcount_tsyn>max_nstep) {
                  restoreInt(bk0);
                  slowdown.restore(bksd0);
                  for (int i=0; i<num; i++) p[i]=p0[i];
                  return -stepcount;
              }
#else
              if(stepcount_tsyn>max_nstep) abort();
#endif
          }

#ifdef ARC_DEEP_DEBUG
          std::cerr<<"Symplectic count: "<<stepcount<<" time (real): "<<slowdown.t_real<<" tend(real): "<<tend_real<<" dterr: "<<(slowdown.t_real-tend_real)/dt_real_full<<" time (int): "<<t;
                   <<" ds_used: "<<ds[dsk]<<" ds_next: "<<ds[1-dsk]
                   <<" Ekin: "<<Ekin
                   <<" Pot: "<<Pot
                   <<" Pt: "<<Pt
                   <<" E+Pt: "<<Ekin+Pot+Pt
                   <<" error: "<<abs((Ekin+Pot+Pt-bk[3]-bk[4]-bk[1])/Pt)
                   <<std::endl;
          std::cerr<<"Timetable: ";
          for (int i=0; i<symk; i++) std::cerr<<" "<<timetable[pars.sym_order[i].index];
          std::cerr<<std::endl;
#endif

#ifdef ARC_WARN
          // accident information
          if(info!=NULL) {
              info->stepcount = stepcount;
              info->ds1 = ds[dsk];
              info->ds2 = ds[1-dsk];

              restoreInt(bk);
              slowdown.restore(bksd);
              break;
          }
#endif

          // energy check
          Float eerr = abs((Ekin+Pot+Pt-bk[3]-bk[4]-bk[1])/bk[1]);
          if(eerr>pars.exp_error) {
              if(fix_step_flag<=1) {
                  //unsigned long Af=to_double(pow(eerr/pars.exp_error,pars.sym_inv_n));
                  Float erat = pars.exp_error/eerr;
                  Float Af=std::max(pow(erat,pars.sym_inv_n),Float(0.125));
                  //unsigned long c=1;
                  //while(Af>0) {
                  //    Af = (Af>>1);
                  //    c = (c<<1);
                  //}
                  //if(c==1) c=2;
                  //Float dsn = ds[dsk]/Float(double(c));
                  //Float dsn = ds[dsk]*(Af>0.8?1.0:Af);

                  //nsub = to_int(1.0/Af)-1;
                  //// if same reduction appear twice, increase counter
                  //if (Afbk==Af) nsubcount++;  
                  //else {
                  //    nsubcount=0;
                  //    Afbk = Af;
                  //}

                  // if nsub not yet reach 0, reset counter
                  // if(nsub>0) nsubcount = 0;
              
                  // check whether next step is already smaller than modified step
                  //if(dsn > ds[1-dsk]) {
                  //    nsub = -1;
                  //    ds[dsk] = ds[1-dsk];
                  //}
                  //else {
                  //    // backup original step
                  //    dsbk = ds[1-dsk];
                  //    ds[1-dsk] = dsn;
                  //    ds[dsk] = dsn;
                  //    nsub = (c<<nsubcount);
                  //}
                  // 
                  // if first step, reduce permanently
                  if(stepcount<3) {
                      nsub=-1;
                      ds[dsk] *=Af;
                      ds[1-dsk] =ds[dsk];
                      bk_flag = false;
#ifdef ARC_DEEP_DEBUG
                      std::cerr<<"Detected energy erro too large eerr/err_max ="<<1.0/erat<<" eerr="<<eerr<<" Af="<<Af<<" substep number="<<nsub<<std::endl;
#endif
                      continue;
                  }
                  // for big energy error
                  else if (fix_step_flag==0&&erat<0.1) {
                      if(bk_flag) dsbk = ds[dsk];
                      ds[dsk] *=Af;
                      ds[1-dsk] = ds[dsk];
                      bk_flag = false;
                      nsub = 2*to_int(1.0/Af);
#ifdef ARC_DEEP_DEBUG
                      std::cerr<<"Detected energy erro too large eerr/err_max ="<<1.0/erat<<" eerr="<<eerr<<" Af="<<Af<<" substep number="<<nsub<<std::endl;
#endif
                      continue;
                  }
              }
          }
#ifdef ARC_WARN
          else if(eerr>100*pars.exp_error) {
              std::cerr<<"Warning: symplectic integrator error > 100*criterion:"<<eerr<<std::endl;
          }
#endif
//          eerr_pre = eerr;

          if(!tend_flag&&dt_real<pars.dtmin) {
              std::cerr<<"Error! symplectic integrated time step ("<<dt_real<<") < minimum step ("<<pars.dtmin<<")!\n";
              std::cerr<<" stepcount: "<<stepcount<<" ds_used: "<<ds[dsk]<<" energy error: "<<abs((Ekin+Pot+Pt-bk[3]-bk[4]-bk[1])/Pt)<<std::endl;
#ifdef ARC_DEBUG_DUMP
              restoreInt(bk0);
              slowdown.restore(bksd0);
              for (int i=0; i<num; i++) p[i]=p0[i];
              return -stepcount;
#else
              abort();
#endif
          }
          
          
          if(slowdown.t_real<tend_real-terr_real){
              // step increase depend on nsub or error
              if(fix_step_flag==0&&!tend_flag) {
                  if(nsub==0) {
#ifdef ARC_DEEP_DEBUG
                      std::cerr<<"Recover to backup step ds_current="<<ds[dsk]<<" ds_next="<<ds[1-dsk]<<" ds_backup="<<dsbk<<std::endl;
#endif
                      ds[1-dsk] = dsbk;
                  }
                  else if(eerr<=eerr_min) {
                      ds[1-dsk] *= pow(eerr_min/std::max(eerr,Float(1e-15)),Float(0.16666666666));
#ifdef ARC_DEEP_DEBUG
                      std::cerr<<"Energy error is small enought for increase step, error="<<eerr<<" limit="<<pars.exp_error<<" factor="<<pow(pars.exp_error/std::max(eerr,1e-15),0.16666666666)<<" sym_An="<<pars.sym_An<<" new ds="<<ds[1-dsk]<<std::endl;
#endif
                  }
                  nsub--;
              }
              if(tend_flag&&ds[dsk]==ds[1-dsk]) {
                  stepcount_tsyn++;
                  Float dtoff_real = tend_real-slowdown.t_real;
                  if(nsubcount>1&&dt_real<0.3*dtoff_real) {
                      ds[1-dsk] = ds[dsk]*dtoff_real/dt_real;
#ifdef ARC_DEEP_DEBUG
                      std::cerr<<"Time step dt(real) "<<dt_real<<" <0.3*(tend-t)(real) "<<dtoff_real<<" enlarge step factor: "<<dtoff_real/dt_real<<" new ds: "<<ds[1-dsk]<<std::endl;
#endif
                  }
                  else nsubcount++;
              }

              // update ds
              ds[dsk] = ds[1-dsk]; // when used once, update to the new step
              dsk = 1-dsk;

              bk_flag = true;
              if(num>2) update_link();
              else {
                  // update rmax and rmin
                  rmin2_ = X[0][0]*X[0][0] + X[0][1]*X[0][1] + X[0][2]*X[0][2];
                  rmax2_ = rmin2_;
              }
          }
          else if(slowdown.t_real>tend_real+terr_real) {
              tend_flag = true;
              bk_flag = false;
              update_kappa_flag = false;
              stepcount_tsyn++;
              nsubcount=0;
              // check timetable
              int i=-1,k=0;
              for(i=0; i<symk; i++) {
                  k = pars.sym_order[i].index;
                  if(tend_real<timetable[k]) break;
              }
              if (i==0) {
                  ds[dsk] *= pars.sym_order[i].cck*tend_real/timetable[k];
                  ds[1-dsk] = ds[dsk];
#ifdef ARC_DEEP_DEBUG
                  std::cerr<<"T-end reach, t1 (real)= "<<timetable[k]<<" t (real)= "<<slowdown.t_real<<" tend/t1 (real)="<<tend_real/timetable[k]<<" ck1="<<pars.sym_order[i].cck<<" ds1 = "<<ds[dsk]<<" ds2 = "<<ds[1-dsk]<<"\n";
#endif
              }
              else {
                  Float tp_real = timetable[pars.sym_order[i-1].index];
                  Float dtk_real = timetable[k] - tp_real;
                  Float dtmp = ds[dsk];
                  ds[dsk] *= pars.sym_order[i-1].cck;  // first set step to nearest k for t<tend
                  ds[1-dsk] = dtmp*(pars.sym_order[i].cck-pars.sym_order[i-1].cck)*std::min(Float(1.0),(tend_real-tp_real+terr_real)/dtk_real); //then set next step to c_k+1 -c_k
#ifdef ARC_DEEP_DEBUG
                  std::cerr<<"T-end reach, t0 (real)= "<<tp_real<<" t1 (real)= "<<timetable[k]<<" t (real)= "<<slowdown.t_real<<" (tend-tp)/dt (real)="<<(tend_real-tp)/dt_real<<" ck1="<<pars.sym_order[i].cck<<" ck0="<<pars.sym_order[i-1].cck<<" ds1 = "<<ds[dsk]<<" ds2 = "<<ds[1-dsk]<<" \n";
#endif
              }
          }
          else {
              if(num>2) update_link();
              else {
                  // update rmax and rmin
                  rmin2_ = X[0][0]*X[0][0] + X[0][1]*X[0][1] + X[0][2]*X[0][2];
                  rmax2_ = rmin2_;
              }
              
#ifdef ARC_DEEP_DEBUG
              std::cerr<<"Finish, terr = "<<(slowdown.t_real - tend_real)/dt_real_full<<" eerr = "<<abs((Ekin+Pot+Pt-bk[3]-bk[4]-bk[1])/Pt)<<std::endl;
#endif
              break;
          }

      }
#ifdef ARC_PROFILE
      if(!profile.stepcount) profile.initstep(1);
      profile.stepcount[0] +=stepcount;
#endif
      return stepcount;
  }

#ifdef ARC_DEBUG
  //! test for whether the integration can be correctly restored to original data
  template<class pertparticle_, class pertforce_, class extpar_>
  void Symplectic_integration_repeat_test( const Float s, 
                                           chainpars &pars,
                                           extpar_ *int_pars = NULL,
                                           pertparticle_* pert = NULL, 
                                           pertforce_* pertf = NULL, 
                                           const int npert = 0) {
      const int dsize  = 6*(num-1)+3;
      const int darray = dsize+1; // backup data array size;
      Float bk[darray]; // for backup
      bk[dsize] = (Float)dsize;    // label for safety check
      const int symk = pars.sym_k;
      Float timetable[symk]; // for storing time information

      backup(bk);
      Float Ekin_bk = Ekin;
      Float Pot_bk  = Pot;
      Float T_real_bk = slowdown.t_real;

#ifdef ARC_OPT_SYM2
      const Float pm1 = p[0].getMass();
      const Float pm2 = p[1].getMass();
      const Float m2_mt = pm2/(pm1+pm2);
      const Float m1_m2 = pm1/pm2;

      if(num==2) Symplectic_integration_two(s, pars, timetable, m2_mt, m1_m2, int_pars, pert, pertf, npert);
      else Symplectic_integration(s, pars, timetable, int_pars, pert, pertf, npert, false);
#else 
      Symplectic_integration(s, pars, timetable, int_pars, pert, pertf, npert, false);
#endif

      Float tbk =t;

      restore(bk);
      Ekin= Ekin_bk;
      Pot= Pot_bk;
      slowdown.t_real = T_real_bk;
      resolve_V();

#ifdef ARC_OPT_SYM2
      if(num==2) Symplectic_integration_two(s, pars, timetable, m2_mt, m1_m2, int_pars, pert, pertf, npert);
      else Symplectic_integration(s, pars, timetable, int_pars, pert, pertf, npert, false);
#else 
      Symplectic_integration(s, pars, timetable, int_pars, pert, pertf, npert, false);
#endif

      if(t!=tbk) {
          std::cerr<<"Error, data is not correctly restored, integratin time twice give different results: "<<t<<" "<<tbk<<" diff="<<t-tbk<<std::endl;
          abort();
      }
      else {
          std::cerr<<"Data can be successfully restored, step = "<<s<<std::endl;
      }

      restore(bk);
      Ekin= Ekin_bk;
      Pot= Pot_bk;
      slowdown.t_real = T_real_bk;
      resolve_V();

    }
#endif

  //! Get current physical time
  /*! \return current physical time
   */
  Float getTime() const {
    return slowdown.t_real;
  }
  //! Get current kinetic energy
  /*! \return current kinetic energy
  */
  Float getEkin() const {
    return Ekin;
  }

  //! Get current potential energy
  /*! \return current potetnial energy (negative value for bounded systems)
  */
  Float getPot() const {
    return Pot;
  }

  //! Get current time momemtum \f$Pt\f$ (current system binding energy)
  /*! \return time momemtum \f$Pt\f$ (current system binding energy \f$(-H(t))\f$)
  */
  Float getPt() const {
    return Pt;
  }
  
  //! Get current integrated time transformation function value \f$w\f$
  /*! \f$w\f$: \f$ \frac{dw}{dt} = \sum_k \frac{\partial W}{\partial \vec{r_k}} \bullet \vec{v_k} \f$
      (see W in getW();)
      \return \f$w\f$
  */
  Float getw() const {
    return w;
  }

  //! Get current time transformation function value \f$W\f$
  /*! The \f$W\f$ is defined in ::ARC::pair_AW().
      \return \f$W\f$
  */
  Float getW() const {
    return W;
  }

  //! Get chain list
  /*! Obtain the chain list index ordered by the nearest distances of particles.
    @param[out] indexlist: integer array to store the chain list (size of num)
   */
  void getList(int* indexlist) {
    std::memcpy(indexlist,list,num*sizeof(int));
  }
  
  //! print chain data
  /*! Print chain data 
      @param[in] fout: ofstream for printing
      @param[in] precision: printed precision for one variable
      @param[in] width: printing width for one variable
  */
  void print(std::ostream & fout, const int precision=15, const int width=23) {
    if (width<=0) {
      fout<<"Error: width should be larger than zero!\n";
      abort();
    }
    char xyz[4]={'x','y','z','r'};
    fout<<std::setprecision(precision);
    fout<<"---- particle list------\n ";
    for (int i=0;i<num;i++) fout<<std::setw(width)<<list[i];
    fout<<"\n----- relative position X ------\n";
    for (int k=0;k<3;k++) {
      fout<<xyz[k];
      for (int i=0;i<num-1;i++) fout<<std::setw(width)<<X[i][k];
      fout<<std::endl;
    }
    fout<<"\n----- relative velocity V ------\n";
    for (int k=0;k<3;k++) {
      fout<<xyz[k];
      for (int i=0;i<num-1;i++) fout<<std::setw(width)<<V[i][k];
      fout<<std::endl;
    }
    fout<<"\n----- Acceleration A ------\n";
    for (int k=0;k<3;k++) {
      fout<<xyz[k];
      for (int i=0;i<num;i++) fout<<std::setw(width)<<acc[i][k];
      fout<<std::endl;
    }
    fout<<"\n----- part omega / part rk ------\n";
    for (int k=0;k<3;k++) {
      fout<<xyz[k];
      for (int i=0;i<num;i++) fout<<std::setw(width)<<dWdr[i][k];
      fout<<std::endl;
    }
    fout<<"\n----- system parameters ------\n"
        <<"---Center-of-mass data: \n"
        <<"mass:"<<std::setw(width)<<particle::getMass()<<std::endl
        <<"(particle*)pos:"
        <<std::setw(width)<<particle::getPos()[0]
        <<std::setw(width)<<particle::getPos()[1]
        <<std::setw(width)<<particle::getPos()[2]<<std::endl
        <<"(particle*)vel:"
        <<std::setw(width)<<particle::getVel()[0]
        <<std::setw(width)<<particle::getVel()[1]
        <<std::setw(width)<<particle::getVel()[2]<<std::endl
        <<"---Energy: "<<std::endl
        <<"Kinetic energy Ekin: "<<std::setw(width)<<Ekin<<std::endl
        <<"Potential energy Pot: "<<std::setw(width)<<Pot<<std::endl
        <<"Transformation factor Omega: "<<std::setw(width)<<W<<std::endl
        <<"---Integration parameters: "<<std::endl
        <<"Physical time: "<<std::setw(width)<<t<<std::endl
        <<"Time momentum Pt: "<<std::setw(width)<<Pt<<std::endl
        <<"Transformation coefficient omega: "<<std::setw(width)<<w<<std::endl;
        //<<"---Time step coefficients: "<<std::endl;
        //<<"alpha: "<<std::setw(width)<<pars.alpha<<std::endl
        //<<"beta: "<<std::setw(width)<<pars.beta<<std::endl
        //<<"gamma: "<<std::setw(width)<<pars.gamma<<std::endl;
    slowdown.print(fout, precision, width);
  }

};

//! Generalized list to store chain and particle members
/*! A list that storing particle memory addresses and their copy (based on template class particle_)
 */
template <class particle_>
class chainlist{
  typedef particle_ particle;
  int num; //!< number of current particles in the list #p
  int nmax; //!< maximum number of particles allown to store
  // int nchain;  //!< number of chain type members
  // bool* cflag; //!< flag array to indicate whether the corresponding particle in #p is chain (true: chain; false: Particle)
  particle** p; //!< particle list array (void pointer array)
  particle* data; //!< copy of particle data
  bool alloc_flag ;//!< indicate whether the p stores the particle memory address (false) or allocates new memory (true)
  // bool read_flag; //!< indicate whether the data is read from file
//  bool change_flag; //!< indicate whether the add/remove is used.

public:
  //! Constructor 
  /*! Set current particle number to zero, need to use allocate() to allocate memory for storing particle addresses, or load() to point to exited particle array, or read() to get particle data from a file
   */
  chainlist(): num(0), nmax(0), p(NULL), data(NULL), alloc_flag(false) {};
  
  //! Constructor with maximum number of particle \a n
  /*! Set maximum particle number to \a n and allocate memory for #p to storing the particle addresses (maximum \a n)
   */
  chainlist(const int n) {
      alloc_flag = false;
      allocate(n); 
  }

  //! Memory allocation of particle and particle address array
  /*! Set maximum particle number to \a n and allocate memory for #p to storing the particle addresses (maximum \a n) and #data to storing particle copies
   */
  void allocate(const int n) {
    num = 0;
    nmax = n;
    // nchain = 0;
    //cflag=new bool[n];
    if(alloc_flag) {
        delete[] p;
        delete[] data;
#ifdef ARC_DEBUG
        std::cerr<<"Warning!: chainlist re-init, all current particle data are lost!\n";
#endif
    }
    p=new particle*[n];
    data=new particle[n];
    alloc_flag = true;
  }

//  //! allocate memory of particles
//  /*! allocate memory of n particle data with type of particle class
//    @param [in] n: number of particles
//   */
//  void allocate(const int n) {
//    if (num>0&&!alloc_flag) {
//      std::cerr<<"Error: the particle list already stores particle addresses, the allocation of new particle memory is forbidden, please clear first!\n";
//      abort();
//    }
//    if(n+num>nmax) {
//      std::cerr<<"Error: particle number allocated ("<<n<<") + current particle number ("<<num<<") > maximum number allown ("<<nmax<<")!\n";
//      abort();
//    }
//    for (int i=0; i<n; i++) {
//      p[i+num] = new particle;
//      cflag[i+num] = false;
//    }
//    num += n;
//    alloc_flag = true;
//  }
  
  //! Clear function
  /*! Free dynamical memory space used in particle address list #p
   */
  void clear() {
    if (alloc_flag) {
      //if (alloc_flag) 
      //  for (int i=0;i<num;i++)
      //    if (p[i]!=NULL) delete (particle*)p[i];
      // delete[] cflag;
      delete[] p;
      delete[] data;
      p = NULL;
      data = NULL;
      alloc_flag =false;
      //alloc_flag = false;
    }
    nmax = 0;
    num = 0;
  }

  chainlist& operator = (const chainlist& _c) {
      clear();
      allocate(_c.nmax);
      num = _c.num;
      for(int i=0; i<num; i++) p[i] = _c.p[i];
      for(int i=0; i<num; i++) data[i] = _c.data[i];
      return *this;
  }


  //! Destructor
  ~chainlist() {
    if (alloc_flag) {
      //if (alloc_flag) 
      //  for (int i=0;i<num;i++)
      //    if (p[i]!=NULL) delete (particle*)p[i];
      // delete[] cflag;
      delete[] p;
      delete[] data;
    }
  }

  //! Get current particle number
  /*! \return Current particle number in particle address list #p
   */
  int getN() const {
    return num;
  }

  //! return particle data first pointer
  particle* getData() const {
    return data;
  }

  //! Get maximum particle number
  /*! \return Current maximum particle number that can be stored in particle address list #p
   */
  int getNmax() const {
    return nmax;
  }
  
  ////! Get number of chain members
  ///*! \return Number of chain members in particle address list #p
  // */
  //int getNchain() const {
  //  return nchain;
  //}

  //! Get particle masses and store into array
  /*! Obtain particle masses and store into the Float array
    @param[in] mass: Float array to store the particle masses
   */
  void getMassAll(Float mass[]) {
    for (int i=0;i<num;i++) mass[i] = (*this)[i].getMass();
  }

  //! Add new particle
  /*! Add new particle address at the end of the particle address list #p (particle type should be derived class from particle_), and copy it to #data
    @param [in] a: new particle
   */
  template <class Tp>
  void add(const Tp &a) {
    //if (alloc_flag) {
    //  std::cerr<<"Error: chainlist already allocate memory for particle list, to avoid confusion, no new particle address can be appended!\n";
    //  abort();
    //}
    //if(alloc_flag) {
    //    std::cerr<<"Error: chainlist already read data from file, no new particle address can be appended!\n";
    //    abort();
    //}
    if (num<nmax) {
      //cflag[num] = false;
      p[num] = (particle*)&a;
      data[num] = a;
      num++;
    }
    else {
      std::cerr<<"Error: chainlist overflow! maximum number is "<<nmax<<std::endl;
      abort();
    }
  }

  //! Add a list of particles
  /*! ADD a list of particles at the end of the particle address list #p and copy it to #data
    (particle type should be derived class from particle_)
    @param [in] n: number of new particles
    @param [in] a: array of new particles
   */
  template<class Tp>
  void add(const int n, Tp a[]) {
    //if(alloc_flag) {
    //    std::cerr<<"Error: chainlist already read data from file, no new particle address can be appended!\n";
    //    abort();
    //}
    //if (alloc_flag) {
    //  std::cerr<<"Error: chainlist already allocate memory for particle list, to avoid confusion, no new particle address can be appended!\n";
    //  abort();
    //}
    if (num+n<=nmax) {
      for (int i=0;i<n;i++) {
          //cflag[num+i] = false;
          p[num+i] = (particle*)&a[i];
          data[num+i] = a[i];
      }
      num +=n;
    }
    else {
      std::cerr<<"Error: chainlist overflow! maximum number is "<<nmax<<", current number "<<num<<", try to add "<<n<<std::endl;
      abort();
    }
  }

  //! load a list of particles
  /*! different from add(), this function will directly register the first particle address and the number of particles. Thus no copies of particles are made. This is used when all particles are continued distributed in memory. This function can not be used when allocate() is already called. 
    @param [in] n: number of new particles
    @param [in] a: array of new particles
   */
  void load(const int n, particle a[]) {
      if(alloc_flag) {
          std::cerr<<"Error: chainlist already allocate memory for particle data, please call clear() first before using load()!\n";
          abort();
      }
#ifdef ARC_DEBUG
      if(data) {
          std::cerr<<"Warning: chainlist already store one particle list address, (current particle number = "<<num<<"), now update to new list with number = "<<n<<"!\n";
      }
#endif
      nmax = num = n;
      data = a;
  }
    
  ////! Add a chain in particle address list #p
  ///*! Add one chain's address at the end of particle address list #p
  //  @param [in] a: new chain particle
  // */
  //void add(chain<particle> &a) {
  //  if(alloc_flag) {
  //      std::cerr<<"Error: chainlist already read data from file, no new particle address can be appended!\n";
  //      abort();
  //  }
  //  //if (alloc_flag) {
  //  //  std::cerr<<"Error: chainlist already allocate memory for particle list, to avoid confusion, no new particle address can be appended!\n";
  //  //  abort();
  //  //}
  //  if (num<nmax) {
  //    cflag[num] = true;
  //    p[num] = &a;
  //    data[num] = a.cm;
  //    num++;
  //    nchain++;
  //  }
  //  else {
  //    std::cerr<<"Error: chainlist overflow! maximum number is "<<nmax<<std::endl;
  //    abort();
  //  }
  //}

  //! remove one particle
  /*! remove one particle from #p
     @param [in] i: particle index in #p needs to be removed
     @param [in] option: position update option: 
                 - true: shift last particle to current position (defaulted);
                 - false: shift all right particle to left by one
  */
  void remove(const int i, bool option=true) {
    if (option) {
      if (i<num-1) {
        // if (cflag[i]) nchain--;
        num--;
        //cflag[i] = cflag[num];
        data[i] = data[num];
        p[i] = p[num];
        //if (alloc_flag) p[num]=NULL;
      }
      else {
        std::cerr<<"Warning!: try to remove non-existing particle (index "<<i<<" > maximum number "<<num<<"std::endl";
      }
    }
    else {
      if (i<num-1) {
        // if (cflag[i]) nchain--;
        num--;
        for (int j=i;j<num;j++) {
          //cflag[j] = cflag[j+1];
          data[j] = data[j+1];
          p[j] = p[j+1];
        }
        //if (alloc_flag) p[num]=NULL;
      }
      else {
        std::cerr<<"Warning!: try to remove non-existing particle (index "<<i<<" > maximum number "<<num<<"std::endl";
      }
    }
  }

  //! Return the i^th of memebr's reference in the particle #data
  /*! [] Operator overloading, return the i^th particle reference from the particle (copied) data list
    @param [in] i: the index of member in the list #p
    \return: particle reference in chainlist #data
   */
  particle &operator [](const int i) const {
    //if (cflag[i]) {
    //  return ((chain<particle>*)p[i])->cm;
    //}
    //else {
    //  return *((particle*)p[i]);
    //}
#ifdef ARC_DEBUG
    if (i>=num) {
        std::cerr<<"Error: the required index "<<i<<" exceed the current particle list boundary (total number = "<<num<<")!"<<std::endl;
        abort();
    }
#endif
    return data[i];
  }

  //! return particle original address array
  /*!
    \return: particle original address array (const)
   */
  particle** getPAdr() const{
      return p;
  }

  //! Dump function for particle data
  /*! Dump particle data into file using fwrite (binary format). Number of particles is written first, then the data of particle class in the particle list #p.
    @param [in] filename: file to store the data
   */
  void dump(const char* filename) {
    std::FILE* pout = std::fopen(filename,"w");
    if (pout==NULL) std::cerr<<"Error: filename "<<filename<<" cannot be open!\n";
    else {
      fwrite(&num,sizeof(int),1,pout);
      for (int i=0; i<num; i++) {
        fwrite(&((*this)[i]),sizeof(particle),1,pout);
      }
      fclose(pout);
    }
  }

  //! Read function for particle data
  /*! Read particle data using fread (binary format). Number of particles should be first variable, then the data of particles (the data structure should be consistent with particle class).
      Notice if read is used, the particle data memory is allocated inside chainlist, this is different from add() function.
    @param [in] filename: file to read the data
   */
  void read(const char* filename) {
    std::FILE* pin = std::fopen(filename,"r");
    if (pin==NULL) std::cerr<<"Error: filename "<<filename<<" cannot be open!\n";
    else {
      int n;
      int rn = fread(&n,sizeof(int),1,pin);
      if(rn<1) {
        std::cerr<<"Error: cannot read particle number!\n";
        abort();
      }

      allocate(n);
      for (int i=0; i<n; i++) {
        rn = fread(&data[i+num],sizeof(particle),1,pin);
        p[i+num]=NULL;
        //cflag[i+num]=false;
        if (rn<1) {
          std::cerr<<"Error: cannot read particle "<<i<<", total particle number is "<<n<<"!\n";
          abort();
        }
      }
      fclose(pin);
    }
  }

  ////! Is i^th a chain?
  ///*! Check whether the i^th member in the particle address list is chain
  //   @param [in] i: member index in list #p
  //   \return  true: chain; false: particle
  // */
  //bool isChain (const int i) const {
  //  return cflag[i];
  //}
  
  ////! Get particle list in a chain type member
  ///*! Return the address of particle list of the i^th member in particle #p (ARC::chain.p)
  //  @param [in] i: member index in list the particle address list #p
  //  \return
  //   - if i^th member in #p is chain, returen the address of particle list (ARC::chain.p).
  //   - else return NULL
  // */
  //chain<particle> *getSub (const int i) const {
  //  if (cflag[i]) return (chain<particle>*)p[i];
  //  else return NULL;
  //}

  //! copy data back to original address
  /*! copy the data back to original address assuming data type as particle_
   */
  void copyback() {
    //if(alloc_flag) {
    //    std::cerr<<"Error: data is read from file, cannot copyback!\n";
    //    abort();
    //}
    if(!alloc_flag) return;
    for(int i=0; i<num; i++) {
        //if(cflag[i]) ((chain<particle>*)p[i])->cm=data[i];
        //else *p[i] = data[i];
#ifdef ARC_DEBUG
        const double* pos=data[i].getPos();
        const double* vel=data[i].getVel();
        assert(!std::isnan(pos[0]));
        assert(!std::isnan(pos[1]));
        assert(!std::isnan(pos[2]));
        assert(!std::isnan(vel[0]));
        assert(!std::isnan(vel[1]));
        assert(!std::isnan(vel[2]));
#endif
        if(p[i]!=NULL) *p[i] = data[i];
    }
  }

  //! show whether the chainlist allocated memory
  /*!
    \return if allocated, ture; otherwise false
   */
  bool is_alloc() const {
      return alloc_flag;
  }

};

}
