#ifndef _FUZZY_H_
#define _FUZZY_H_

enum Linguistic_var {
  NL, NS, Z, PS, PL,
  OFF, VL, L, M, H, VH,
  HALF, OPENED, CLOSED
};



class triangular_mf
{
public:
  float a1;
  float a2;
  float a_m;

  float get_tri_mf_value(triangular_mf mf, float input);
  float get_output_tri_mf_values(triangular_mf mf, float mf_deg ,float &n_val1, float &n_val2);

};

class trapezoidal_mf
{
public:

  float a1;
  float a1_1;
  float a2;
  float a2_1;

  float get_trap_mf_value(trapezoidal_mf mf, float input);
  float get_output_trap_mf_values(trapezoidal_mf mf, float mf_deg, float &n_val1, float &n_val2);
};

class half_T_mf
{
public:

  float a1,a2;

  float get_half_T_mf_value(half_T_mf mf, float input);
  float get_output_half_T_mf_value(half_T_mf mf, float mf_deg, float &var);
};

class L_mf
{
public:
  float a1,a2;

  float get_L_mf_value(L_mf mf, float input);
  float get_output_L_mf_value(L_mf mf, float mf_deg, float &var);
};



#endif
