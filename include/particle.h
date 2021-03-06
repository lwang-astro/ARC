#pragma once
#include <cassert>
#include <cstring>
#include "Float.h"

#ifndef NAN_CHECK
#define NAN_CHECK(val) assert((val) == (val));
#endif

//! A sample particle class used for \ref ARC::chain
/*! Notice for the template class of \ref ARC::chain, the supported particle class should contain the public member functions: setPos(), setVel(), setMass(), getPos(), getVel(), getMass() as described in the document
 */
class Particle{
  Float pos[3], vel[3];
  Float mass, coff;

public:
  //!Constructor without initialization
  /*! Notice the position velocity and mass are random values in this case
   */
  Particle() {} 

  //!Constructor with initialization of mass, position and velocity
  /*! @param [in] m: particle mass
      @param [in] r: particle position vector
      @param [in] v: particle velocity vector
   */
  Particle(const Float m, const Float r[3], const Float v[3]) {
    set(m,r,v);
    coff = m;
  }

  //!Constructor with initialization of mass, position and velocity
  /*! @param [in] m: particle mass
      @param [in] rx: particle position in x axis
      @param [in] ry: particle position in y axis
      @param [in] rz: particle position in z axis
      @param [in] vx: particle velocity in x axis
      @param [in] vy: particle velocity in y axis 
      @param [in] vz: particle velocity in z axis 
   */
  Particle(const Float m, const Float rx, const Float ry, const Float rz, const Float vx, const Float vy, const Float vz) {
    set(m,rx,ry,rz,vx,vy,vz);
    coff = m;
  }

  //!Constructor with initialization based on another Particle type data
  /*! @param [in] a: Particle type of data used to obtain mass, position and velocity
   */
  Particle(const Particle &a) {
    set(a);
  }

  //! Get mass (required for \ref ARC::chain)
  /*! \return mass
   */
  Float getMass() const{
    return mass;
  }
  
  //! Get position (required for \ref ARC::chain)
  /*! \return position vector (Float[3])
   */
  const Float* getPos() const{
    return pos;
  }

  //! Get velocity (required for \ref ARC::chain)
  /*! \return velocity vector (Float[3])
   */
  const Float* getVel() const{
    return vel;
  }

  //! Get cofficients for TTL method
  /*! \return cofficient
   */
  Float getCoff() const{
    return coff;
  }
  
  //!Set particle data with mass, position and velocity
  /*! Set particle mass, position and velocity, NAN check will be done
      @param [in] m: particle mass
      @param [in] rx: particle position in x axis
      @param [in] ry: particle position in y axis
      @param [in] rz: particle position in z axis
      @param [in] vx: particle velocity in x axis
      @param [in] vy: particle velocity in y axis 
      @param [in] vz: particle velocity in z axis 
   */
  void set(const Float m, const Float rx, const Float ry, const Float rz, const Float vx, const Float vy, const Float vz){
    NAN_CHECK(m);
    NAN_CHECK(rx);
    NAN_CHECK(ry);
    NAN_CHECK(rz);
    NAN_CHECK(vx);
    NAN_CHECK(vy);
    NAN_CHECK(vz);

    mass=m;
    coff=m;
    pos[0]=rx;
    pos[1]=ry;
    pos[2]=rz;
    vel[0]=vx;
    vel[1]=vy;
    vel[2]=vz;
  }

  //!Set particle data with mass, position and velocity vectors
  /*! Set particle mass, position and velocity, NAN check will be done
      @param [in] m: particle mass
      @param [in] r: particle position vector
      @param [in] v: particle velocity vector
   */
  void set(const Float m, const Float r[3], const Float v[3]) {
    NAN_CHECK(m);
    NAN_CHECK(r[0]);
    NAN_CHECK(r[1]);
    NAN_CHECK(r[2]);
    NAN_CHECK(v[0]);
    NAN_CHECK(v[1]);
    NAN_CHECK(v[2]);

    mass=m;
    coff=m;
    std::memcpy(pos,r,3*sizeof(Float));
    std::memcpy(vel,v,3*sizeof(Float));
}
    
  //!Set particle data with Particle
  /*! @param [in] a: Particle type of data used to obtain mass, position and velocity
   */
  void set(const Particle &a){
    mass = a.getMass();
    coff = a.getCoff();
    std::memcpy(pos,a.getPos(),3*sizeof(Float));
    std::memcpy(vel,a.getVel(),3*sizeof(Float));
  }

  //!Set position (required for \ref ARC::chain)
  /*! NAN check will be done
      @param [in] x: particle position in x axis
      @param [in] y: particle position in y axis
      @param [in] z: particle position in z axis
   */
  void setPos(const Float x, const Float y, const Float z) {
    NAN_CHECK(x);
    NAN_CHECK(y);
    NAN_CHECK(z);
    
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
  }

  //!Set position with vector
  /*! NAN check will be done
      @param [in] r: particle position vector
   */
  void setPos(const Float r[3]) {
    NAN_CHECK(r[0]);
    NAN_CHECK(r[1]);
    NAN_CHECK(r[2]);

    std::memcpy(pos,r,3*sizeof(Float));
  }

  //!Set velocity (required for \ref ARC::chain)
  /*! NAN check will be done
      @param [in] vx: particle velocity in x axis
      @param [in] vy: particle velocity in y axis 
      @param [in] vz: particle velocity in z axis 
  */
  void setVel(const Float vx, const Float vy, const Float vz) {
    NAN_CHECK(vx);
    NAN_CHECK(vy);
    NAN_CHECK(vz);
    
    vel[0] = vx;
    vel[1] = vy;
    vel[2] = vz;
  }

  //!Set position with vector
  /*! NAN check will be done
      @param [in] v: particle velocity vector
   */
  void setVel(const Float v[3]) {
    NAN_CHECK(v[0]);
    NAN_CHECK(v[1]);
    NAN_CHECK(v[2]);

    std::memcpy(vel,v,3*sizeof(Float));
  }

  //!Set mass (required for \ref ARC::chain)
  /*! NAN check will be done
      @param [in] m: particle mass
   */
  void setMass(const Float m) {
    NAN_CHECK(m);

    mass = m;
  }

  //!Set coff (required for \ref ARC::chain)
  /*! NAN check will be done
      @param [in] c: cofficient
   */
  void setCoff(const Float c) {
    NAN_CHECK(c);

    coff = c;
  }

    //! write class data to file with binary format
    /*! @param[in] _fp: FILE type file for output
     */
    void writeBinary(FILE *_fp) const {
        fwrite(this, sizeof(*this),1,_fp);
    }

    //! read class data to file with binary format
    /*! @param[in] _fp: FILE type file for reading
     */
    void readBinary(FILE *_fin) {
        size_t rcount = fread(this, sizeof(*this),1,_fin);
        if (rcount<1) {
            std::cerr<<"Error: Data reading fails! requiring data number is 1, only obtain "<<rcount<<".\n";
            abort();
        }
    }

    
  //! Clear function
  /*! Reset mass, position and velocity to zero.
   */
  void clear(){
    std::memset(this,0,7*sizeof(Float));
  }
  
};
