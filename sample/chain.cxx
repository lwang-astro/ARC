#include "AR.h"
#include "particle.h"
#include <iostream>
#include <fstream>
//#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <iomanip>
#include <cmath>

void chain_print(const ARC::chain<Particle> &c, const Particle p[], const double ds, const int n, const double w, const double pre) {
  // printing digital precision
  std::cout<<std::setprecision(pre);

  std::cout<<c.getTime()
           <<std::setw(w)<<(c.getEkin()+c.getPot()+c.getPt())/c.getPt()
           <<std::setw(w)<<c.getEkin()
           <<std::setw(w)<<c.getPot()
           <<std::setw(w)<<c.getPt()
           <<std::setw(w)<<c.getw()
           <<std::setw(w)<<c.getW();
  for (int j=0;j<n;j++) {
    std::cout<<std::setw(w)<<p[j].getMass();
    for (int k=0;k<3;k++) {
      std::cout<<std::setw(w)<<p[j].getPos()[k];
    }
    for (int k=0;k<3;k++) {
      std::cout<<std::setw(w)<<p[j].getVel()[k];
    }
  }
  std::cout<<std::setw(w)<<ds;
  std::cout<<std::endl;
}  

int main(int argc, char **argv){
  typedef double double3[3];
  std::size_t n=3; //particle number
  int w=18; //print width
  int pre=10; //print digital precision
  int nstep=1000; // total step size
  int nsubstep=128;  // sub-step number if direct LF method is used
  int itermax=20;  //iteration maximum number for extrapolation methods
  char* sw=NULL;        // if not 'none', use extrapolation method, 'linear' for polynomial method; 'rational'  for rational interpolation method
  char* sq=NULL;        // extrapolation sequence, 'rom' for {h,h/2,h/4,h/8...}; 'bs' for {h,h/2,h/3,h/4,h/6,h/8...}; '4k' for {h/2, h/6, h/10, h/14 ...}; 'hm' for {h, h/2, h/3, h/4 ...}
  char* method=NULL;   // regularization methods, 'logh': Logarithmic Hamitonian; 'ttl': Time-transformed Leapfrog\n (logh)
  double err=1e-10; // phase error requirement
  double terr=1e-12; // time synchronization error
  double s=0.5;    // step size
  double dtmin=5.4e-20; // mimimum physical time step
  double t=0.0;    // initial physical time
  double tend=-1; // ending physical time
  int copt;
  bool fflag=false; //external force flag
  double3* f=NULL;  // external force vectors
  int dsA=0;   //adjust step size switcher
  bool iterfix=false; //if true, iteration times is fixed to itermax 


  static struct option long_options[] = {
    {"t-start", required_argument, 0, 0},
    {"t-end", required_argument, 0, 't'},
    {"nsub", required_argument, 0, 0},
    {"AR-method",required_argument, 0, 'r'},
    {"extra-method",required_argument, 0, 'm'},
    {"extra-seq",required_argument, 0, 'q'},
    {"iter-max",required_argument, 0, 0},
    {"error",required_argument, 0, 'e'},
    {"t-error",required_argument, 0, 0},
    {"dtmin",required_argument, 0, 'd'},
    {"print-width",required_argument, 0, 0},
    {"print-precision",required_argument, 0, 0},
    {"help",no_argument, 0, 'h'},
    {0,0,0,0}
  };
  
  int option_index;
  while ((copt = getopt_long(argc, argv, "N:n:t:s:a:r:m:q:i:e:d:fh", long_options, &option_index)) != -1)
    switch (copt) {
    case 0:
#ifdef DEBUG
      std::cerr<<"option "<<option_index<<" "<<long_options[option_index].name<<" "<<optarg<<std::endl;
#endif
      switch (option_index) {
      case 0:
        t = atof(optarg);
        break;
      case 2:
        nsubstep = atoi(optarg);
        break;
      case 8:
        terr = atof(optarg);
        break;
      case 10:
        w = atof(optarg);
        break;
      case 11:
        pre = atoi(optarg);
        break;
      default:
        std::cerr<<"Unknown option. check '-h' for help.\n";
        abort();
      }
      break;
    case 'N':
      n = atoi(optarg);
      break;
    case 'n':
      nstep = atoi(optarg);
      break;
    case 't':
      tend = atof(optarg);
      break;
    case 's':
      s = atof(optarg);
      break;
    case 'a':
      dsA = atoi(optarg);
      break;
    case 'r':
      method = optarg;
      if (strcmp(method,"logh")&&strcmp(method,"ttl")) {
        std::cerr<<"Regularization method "<<method<<" not found!\n";
        abort();
      }
      break;
    case 'm':
      sw = optarg;
      if (strcmp(sw,"linear")&&strcmp(sw,"rational")&&strcmp(sw,"none")) {
        std::cerr<<"Extrapolation method "<<sw<<" not found!\n";
        abort();
      }
      break;
    case 'q':
      sq = optarg;
      if (strcmp(sq,"rom")&&strcmp(sq,"bs")&&strcmp(sq,"4k")&&strcmp(sq,"hm")) {
        std::cerr<<"Extrapolation sequences "<<sq<<" not found!\n";
        abort();
      }
      break;
    case 'i':
      itermax = atoi(optarg);
      break;
    case 'e':
      err = atof(optarg);
      break;
    case 'd':
      dtmin = atof(optarg);
      break;
    case 'f':
      fflag = true;
      break;
    case 'h':
      std::cout<<"chain [option] data_filename\n"
               <<"Input data file format: each line: mass, x, y, z, vx, vy, vz\n"
               <<"Options: (*) show defaulted values\n"
               <<"    -N [int]:     total number of particles ("<<n<<")\n"
               <<"    -n [int]:     number of integration steps ("<<nstep<<")\n"
               <<"          --t-start [double]:  initial physical time ("<<t<<")\n"
               <<"    -t [double]:  ending physical time; if set, -n will be invalid (unset)\n"
               <<"          --t-end (same as -t)\n"
               <<"    -s [double]:  step size, not physical time step ("<<s<<")\n"
               <<"          --nsub [int]:       sub-step number if no extrapolation method is used ("<<nsubstep<<")\n"
               <<"    -a [int]:     using auto-step adjustment for extrapolation integration (defaulted not switched on)\n"
               <<"                  0: no auto-step\n"
               <<"                  1: use extrapolation error to estimate next step\n"
               <<"                  2: use min(X/(gV),V/(gA)) to estimate next step\n"
               <<"    -r [string]:  algorithmic regularization method (logh)\n"
               <<"                  'logh': Logarithmic Hamitonian\n"
               <<"                  'ttl': Time-transformed Leapfrog\n"
               <<"          --AR-method (same as -r)\n"
               <<"    -m [string]:  use extrapolation method to get high accuracy (rational)\n"
               <<"                  'linear':   polynomial interpolation method;\n"
               <<"                  'rational': rational interpolation method;\n"
               <<"                  'none':     no extrapolation\n"
               <<"          --extra-method (same as -m)\n"
               <<"    -q [string]: extrapolation sequences (bs)\n"
               <<"                  'rom': Romberg sequence {h, h/2, h/4, h/8 ...};\n"
               <<"                  'bs':  Bulirsch & Stoer sequence {h, h/2, h/3, h/4, h/6, h/8 ...}\n"
               <<"                  '4k':  4k linear sequence {h/2, h/6, h/10, h/14 ...}\n"
               <<"                  'hm':  Harmonic sequence {h, h/2, h/3, h/4 ...}"
               <<"          --extra-seq (same as -q)\n"
               <<"    -i [int]: maximum iteration steps for extrapolation method ("<<itermax<<")\n"
               <<"          --iter-max (same as -i)\n"
               <<"    -e [double]:  phase and energy error limit ("<<err<<")\n"
               <<"          --error (same as -e)\n"
               <<"          --t-error [double]: time synchronization error limit ("<<terr<<")\n"
               <<"    -d [double]: [double]:  minimum physical time step ("<<dtmin<<")\n"
               <<"          --dtmin (same as -d)\n"
               <<"    -f :          use constant external force for each particles (read fx, fy, fz after reading each particle data in data file)\n"
               <<"          --print-width [int]:     print width of value ("<<w<<")\n"
               <<"          --print-precision [int]:     print digital precision ("<<pre<<")\n"
               <<"    -h :          print option information\n"
               <<"          --help (same as -h)\n";
      return 0;
    default:
      std::cerr<<"Unknown argument. check '-h' for help.\n";
      abort();
    }

  if (argc==1) {
    std::cerr<<"Please provide particle data filename\n";
    abort();
  }

#ifdef DEBUG
  // parameter list
  std::cerr<<"Options:\n"
           <<"N: "<<n<<std::endl
           <<"steps: "<<nstep<<std::endl
           <<"t-start: "<<t<<std::endl
           <<"t-end: "<<tend<<std::endl
           <<"step size: "<<s<<std::endl
           <<"sub steps: "<<nsubstep<<std::endl
           <<"adjust step size: "<<dsA<<std::endl
           <<"AR-method: "<<(method?method:"logH")<<std::endl
           <<"extra-method: "<<(sw?sw:"rational")<<std::endl
           <<"extra-seq: "<<(sq?sq:"bs")<<std::endl
           <<"itermax: "<<itermax<<std::endl
           <<"error: "<<err<<std::endl
           <<"terr: "<<terr<<std::endl
           <<"dtmin: "<<dtmin<<std::endl
           <<"extra-force: "<<fflag<<std::endl
           <<"print width: "<<w<<std::endl
           <<"print precision: "<<pre<<std::endl;
#endif

  // data file name
  char* filename = argv[argc-1];      

  // open data file
  std::fstream fs;
  fs.open(filename,std::fstream::in);
  if(!fs.is_open()) {
    std::cerr<<"Error: Filename "<<filename<<" not found\n";
    abort();
  }

  // chain controller
  ARC::chainpars pars;
  // pair_AW extra parameter[2], first is used for smooth mass coefficient control, second is used for adjustable coefficient for smooth mass coefficient
  double smpars[2];
  if (method) {
    if (strcmp(method,"ttl")==0) {
      smpars[1]=-1; // switch off smooth mass in TTL method to avoid NAN
      pars.setabg(0.0,1.0,0.0);
    }
    else smpars[1]=0.003;
  }

  // sequence selection
  int msq=2;
  if (sq) {
    if (strcmp(sq,"rom")==0) msq=1;
    else if (strcmp(sq,"bs")==0) msq=2;
    else if (strcmp(sq,"4k")==0) msq=3;
    else msq=4;
  }

  // extrapolation method selection
  int ms=2;
  if (sw) {
    if (strcmp(sw,"linear")==0) ms=1;
    else if (strcmp(sw,"none")==0) ms=0;
  }

  // fix iteration flag, switch on if auto-step adjustment is used
  if (dsA) {
    iterfix=true;
    pars.setAutoStep(dsA);
  }
  
  // set extrapolation parameter 
  pars.setEXP(err,dtmin,terr,itermax,ms,msq,iterfix);

  // new chain class
  ARC::chain<Particle> c(n,pars);

  // reading particle data
  Particle *p=new Particle[n];
  if (fflag) f=new double3[n];
  for (std::size_t i=0;i<n;i++) {
    double x,y,z,vx,vy,vz,m;
    fs>>m>>x>>y>>z>>vx>>vy>>vz;
    if (fflag) fs>>f[i][0]>>f[i][1]>>f[i][2];
    if (fs.eof()) {
      std::cerr<<"Error: data file reach end when reading particles (current loaded particle number is "<<i<<"; required N = "<<n<<std::endl;
      abort();
    }
    p[i]=Particle(m,x,y,z,vx,vy,vz);
  }
  c.addP(n,p);

  // set pair_AW parameter address
  c.Int_pars=smpars;
  // calculate smooth mass coefficient
  smpars[0] = ARC::calc_mm2<Particle>(c.p); //mm2

  // initialization of chain system
  c.init(t);

  //printing column title
  std::cout<<"Time"
           <<std::setw(w)<<"E_err"
           <<std::setw(w)<<"Ekin"
           <<std::setw(w)<<"Pot"
           <<std::setw(w)<<"B"
           <<std::setw(w)<<"w"
           <<std::setw(w)<<"W"    
           <<std::setw(w)<<" "<<"mass-x-y-z-vx-vy-vz-for-each-particles"<<std::endl;

  // integration step counter
  int i=0;

  // step size
  double ds = s;

  // printing data
  chain_print(c,p,0,n,w,pre);

  // integration loop
  while (true) {
#ifdef DEBUG
    std::cerr<<"Time error: "<<c.getTime()-tend<<std::endl;
#endif
    // if reaching ending time or maximum integration step number, stop
    if ((tend<0&&i==nstep)||(tend>0&&std::abs(c.getTime()-tend)<terr)) break;

    // increasing integration counter
    i++;

    // Extrapolation integration
    if (ms) {
      double dsf=c.extrapolation_integration(ds,tend,f);
      // indicator whether ending time is reached, if so, modify ds
      if (dsf<0) ds *= -dsf;
      // auto-adjust step size
      else if (dsA) {
        if (dsf==0) {
          std::cerr<<"Ds too large"<<std::endl;
          dsf=c.extrapolation_integration(0.01*ds,tend,f,true);
          chain_print(c,p,0.01*ds,n,w,pre);
          if (dsf<0) ds *= -dsf;
        }
        else {
          chain_print(c,p,ds,n,w,pre);
          ds = ds*dsf;
        }
      }
      else chain_print(c,p,ds,n,w,pre);
    }
    // Leapfrog integration
    else {
      c.Leapfrog_step_forward(s,nsubstep,f);
      chain_print(c,p,s,n,w,pre);
    }
#ifdef TIME_PROFILE
    std::cerr<<"Time_profile: Step: "<<i<<"  Acc+Pot(s): "<<c.profile.t_apw<<"  Update_link(s): "<<c.profile.t_uplink<<"  Leap-frog(s): "<<c.profile.t_lf<<"  Extrapolation(s): "<<c.profile.t_ep<<"  Perturbation(s): "<<c.profile.t_pext<<"  Dense_output(s): "<<c.profile.t_dense<<std::endl;
#endif
  }
  
#ifdef TIME_PROFILE
  // if extrapolation method is used, counting the iteration (maximum sequence index) level
  if (ms) {
    int* step=new int[itermax+1];
    // Romberg (even) sequence {h, h/2, h/4, h/8 ...}
    if (msq==1) EP::seq_Romberg(step,itermax+1);
    // Bulirsch & Stoer sequence {h, h/2, h/3, h/4, h/6, h/8 ...}
    else if (msq==2) EP::seq_BS(step,itermax+1);
    // E. Hairer (4k) sequences {h, h/2, h/6, h/10, h/14 ...}
    else if (msq==3) EP::seq_Hairer(step,itermax+1);
    // Harmonic sequences {h, h/2, h/3, h/4 ...}
    else EP::seq_Harmonic(step,itermax+1);
    
    int* stepcount = c.profile.stepcount;
    std::cerr<<"Step histogram:\n I\tCount\tNstep\tStepsum\n";
    int stepsum=0;
    int itersum=0;
    for (int i=1;i<=itermax;i++) {
      stepsum += step[i-1];
      std::cerr<<i<<"\t"<<stepcount[i]<<"\t"<<step[i-1]<<"\t"<<stepsum<<std::endl;
      itersum += std::max(stepcount[i]*(i-1),0);
    }
    std::cerr<<"Sum-of-steps: "<<c.profile.itercount<<" Sum-of-iterations: "<<itersum<<std::endl;
  }
#endif

  delete[] p;
  if (fflag) delete[] f;
  
  return 0;
}

