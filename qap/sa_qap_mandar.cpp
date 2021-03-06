#include <iostream>
#include <fstream>
#include <math.h>
#include <time.h>
#include <string> 
#include <stdlib.h>
#include <sstream>

using namespace std;

/****************************************************************/
/*
    This programme implement a simulated annealing for the
    quadratic assignment problem along the lines describes in
    the article D. T. Connoly, "An improved annealing scheme for 
    the QAP", European Journal of Operational Research 46, 1990,
    93-100.

    Compiler : g++ or CC should work. 

    Author : E. Taillard, 
             EIVD, Route de Cheseaux 1, CH-1400 Yverdon, Switzerland

    Date : 16. 3. 98

    Format of data file : Example for problem nug5 :

5

0 1 1 2 3
1 0 2 1 2
1 2 0 1 2     // Flow
2 1 1 0 1
3 2 2 1 0

0 5 2 4 1
5 0 3 0 2
2 3 0 0 0     // Cost, Distance
4 0 0 0 5
1 2 0 5 0

   Additionnal parameters : Number of iterations, number of runs

*/

/********************************************************************/

    
const long n_max = 851;
const long infini = 1399999999;
const long nb_iter_initialisation = 1000; // Connolly proposes nb_iterations/100

typedef long  type_vecteur[n_max];
typedef long type_matrice[n_max][n_max];

type_vecteur best_placement;
long best_cost;

/*--------------- choses manquantes -----------------*/
enum booleen {faux, vrai};


long max(long a, long b) {if (a > b) return(a); else return(b);};
double max(double a, double b) {if (a > b) return(a); else return(b);}
long min(long a, long b) {if (a < b) return(a); else return(b);}
double min(double a, double b) {if (a < b) return(a); else return(b);}
void swap(long &a, long &b) {long temp = a; a = b; b = temp;}      // swap the arguments which are taken by reference

double temps() {return(double(clock())/double(1000));}

void a_la_ligne(ifstream & fichier_donnees)
{char poubelle[1000]; fichier_donnees.getline(poubelle, sizeof(poubelle));}
/*-------------------------------------------------*/

/************* random number generators ****************/

const long m = 2147483647; const long m2 = 2145483479; 
const long a12 = 63308; const long q12 = 33921; const long r12 = 12979; 
const long a13 = -183326; const long q13 = 11714; const long r13 = 2883; 
const long a21 = 86098; const long q21 = 24919; const long r21 = 7417; 
const long a23 = -539608; const long q23 = 3976; const long r23 = 2071;
const double invm = 4.656612873077393e-10;
long x10 = 12345, x11 = 67890, x12 = 13579, 
     x20 = 24680, x21 = 98765, x22 = 43210;

double mon_rand()                                         // Random number generator
 {long h, p12, p13, p21, p23; 
  h = x10/q13; p13 = -a13*(x10-h*q13)-h*r13;
  h = x11/q12; p12 = a12*(x11-h*q12)-h*r12;
  if (p13 < 0) p13 = p13 + m; if (p12 < 0) p12 = p12 + m;
  x10 = x11; x11 = x12; x12 = p12-p13; if (x12 < 0) x12 = x12 + m;
  h = x20/q23; p23 = -a23*(x20-h*q23)-h*r23;
  h = x22/q21; p21 = a21*(x22-h*q21)-h*r21;
  if (p23 < 0) p23 = p23 + m2; if (p21 < 0) p21 = p21 + m2;
  x20 = x21; x21 = x22; x22 = p21-p23; if(x22 < 0) x22 = x22 + m2;
  if (x12 < x22) h = x12 - x22 + m; else h = x12 - x22;
  if (h == 0) return(1.0); else return(h*invm);
 }

long unif(long low, long high)                          // Uniform random number generator
 {return(low + long(double(high - low + 1) *  mon_rand() ));
 }

/************************** sa for qap ********************************/

// Read file contents in matrices A and B of size n
// Here n is global
void lire(long &n, type_matrice &a, type_matrice &b, int pg_num)
 {ifstream fichier_donnees;
  char nom_fichier[30];
  long i, j;

  string file_start = "data/qap_data_pg";
  string file_end = ".txt";
  // string pg_num_string = to_string(pg_num);
  stringstream stemp;
  stemp << pg_num;
  string pg_num_string = stemp.str();

  string file_name = file_start+pg_num_string+file_end;

//  cout << "Data file name : \n";
//  cin >> nom_fichier;
//  cout << file_name << '\n';
  fichier_donnees.open( file_name.c_str() );
  fichier_donnees >> n; a_la_ligne(fichier_donnees);
  for (i = 1; i <= n; i = i+1) for (j = 1; j <= n; j = j+1)
    fichier_donnees >> a[i][j];
  for (i = 1; i <= n; i = i+1) for (j = 1; j <= n; j = j+1)
    fichier_donnees >> b[i][j];
  fichier_donnees.close();
 }

// In the given permutation p, swap location r and s
// Return updated change in cost = dist*flow
long calc_delta_complet2(long n, type_matrice & a, type_matrice & b,
                         type_vecteur & p, long r, long s)
 {long d;
  d = (a[r][r]-a[s][s])*(b[p[s]][p[s]]-b[p[r]][p[r]]) +
      (a[r][s]-a[s][r])*(b[p[s]][p[r]]-b[p[r]][p[s]]);
  for (long k = 1; k <= n; k = k + 1) if (k!=r && k!=s)
    d = d + (a[k][r]-a[k][s])*(b[p[k]][p[s]]-b[p[k]][p[r]]) +
            (a[r][k]-a[s][k])*(b[p[s]][p[k]]-b[p[r]][p[k]]);
  return(d);
 }

// Calculate cost for that particular p with A and B matrices given
long calcule_cout(long n, type_matrice & a, type_matrice & b, type_vecteur & p)
 {long i, j;
  long c = 0;
  for (i = 1; i <= n; i = i + 1) for (j = 1; j <= n; j = j + 1)
    c = c + a[i][j] * b[p[i]][p[j]];
  return(c);
 }

// Get a new permutation p for each of the different solution or retries
void tire_solution_aleatoire(long n, type_vecteur  & p)
 {long i;
  for (i = 1; i <= n; i = i+1) p[i] = i;
  for (i = 2; i <= n; i = i+1) swap(p[i-1], p[unif(i-1, n)]);
 }

// ****************************************************************************
// ************************* Final Solver Function ****************************
// ****************************************************************************
// This solves for each retry, this is simply a solver which takes number of 
// iterations as one of the input

void recuit(long n, type_matrice & a, type_matrice & b,
            type_vecteur & meilleure_sol, long & meilleur_cout,
            long nb_iterations)

 {type_vecteur p;
  long i, r, s;
  long delta;
  double cpu = temps();
  long k = n*(n-1)/2, mxfail = k, nb_fail, no_iteration;
  long dmin = infini, dmax = 0;
  double t0, tf, beta, tfound, temperature;

  for (i = 1; i <= n; i = i + 1) 
    p[i] = meilleure_sol[i];  // an initial solution generated by uniform permutation
  long Cout = calcule_cout(n, a, b, p);
  meilleur_cout = Cout;       // an initial cost by uniform solution generated


  // nb_iter_initialisation is global variable initialized above
  // Right now to 1000
  for (no_iteration = 1; no_iteration <= nb_iter_initialisation;
       no_iteration = no_iteration+1)
   {r = unif(1, n);
    s = unif(1, n-1);
    if (s >= r) s = s+1;

    delta = calc_delta_complet2(n,a,b,p,r,s);
    if (delta > 0)
     {dmin = min(dmin, delta); dmax = max(dmax, delta);}; 
    Cout = Cout + delta;
    swap(p[r], p[s]);
   };

  t0 = dmin + (dmax - dmin)/10.0;
  tf = dmin;
  beta = (t0 - tf)/(nb_iterations*t0*tf);

  nb_fail = 0;
  tfound = t0;
  temperature = t0;
  r = 1; s = 2;
  for (no_iteration = 1; 
       no_iteration <= nb_iterations - nb_iter_initialisation; 
       no_iteration = no_iteration + 1)
    { temperature = temperature / (1.0 + beta*temperature);

      s = s + 1;
      if (s > n)
       {r = r + 1; 
        if (r > n - 1) r = 1;
        s = r + 1;
       };

      delta = calc_delta_complet2(n,a,b,p,r,s);
      if ((delta < 0) || (mon_rand() < exp(-double(delta)/temperature)) ||
           mxfail == nb_fail)
       {Cout = Cout + delta; swap(p[r], p[s]); nb_fail = 0;}
      else nb_fail = nb_fail + 1;

      if (mxfail == nb_fail) {beta = 0; temperature = tfound;};
      if (Cout < meilleur_cout)
       {meilleur_cout = Cout;
        for (i = 1; i <= n; i = i + 1) meilleure_sol[i] = p[i];
        tfound = temperature;
//        cout << "Iteration = " << no_iteration  
//             << "  Cost = " << meilleur_cout 
//             << "  Computational time = " << temps() - cpu <<  '\n';
       };
 
   };

//  cout << "Best solution found : \n";
//  for (i = 1; i <= n; i = i + 1) cout << meilleure_sol[i] << ' ';
//  cout << "\nCost of this solution is:";
//  cout << meilleur_cout ;
//  cout << '\n';

  if (meilleur_cout < best_cost) {
    best_cost = meilleur_cout;
    for (i = 1; i <= n; i = i + 1) best_placement[i] = meilleure_sol[i];
  }
 }


long  n, nb_iterations, nb_res, no_res;
long Cout;
type_matrice a, b;
type_vecteur p;

int main( int argc, char *argv[])
 {
  nb_iterations = 5000;
  nb_res = 20;
//  cout << "nr iterations :" << nb_iterations << endl;
//  cout << "nr resolutions :" << nb_res << endl;

  best_cost = 9223372036854775807 ; //LONG MAX VALUE
  int pg_num = atoi(argv[1]);
  lire(n, a, b, pg_num);

  for (no_res = 1; no_res <= nb_res; no_res = no_res + 1)
   {tire_solution_aleatoire(n, p);
    recuit(n,a,b,p,Cout, nb_iterations);
   };

//  cout << " " << "\n" ;
//  cout << "Final Placement: \n";
//  cout << "Best cost: " << best_cost << "\n";
//  cout << "Best solution found : \n";
//  for (int i = 1; i <= n; i = i + 1) cout << best_placement[i] << ' ';
//  cout << "\n";

  ofstream file_sol;

  string file_start = "result/qap_sol_pg";
  string file_end = ".txt";

//  string pg_num_string = to_string(pg_num);

  stringstream sstr_temp;
  sstr_temp << pg_num;
  string pg_num_string = sstr_temp.str();

  string file_name = file_start+pg_num_string+file_end;

//  cout << "Solution file name : \n";
//  cout << file_name << '\n';
  file_sol.open( file_name.c_str() );

  for (int i=1; i<= n; i = i + 1) file_sol << best_placement[i] << ' ';

  file_sol.close();

  return 0;
 }

