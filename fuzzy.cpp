#include "fuzzy.h"



/******************************************************************************/

float triangular_mf::get_tri_mf_value(triangular_mf mf, float input)
{
  float mf_degree;

  if ((mf.a1 <= input) && (input <= mf.a_m))
  {
    mf_degree = (input - mf.a1)/(mf.a_m - mf.a1);
    return mf_degree;
  }
  else if ((mf.a_m <= input) && (input <= mf.a2))
  {
    mf_degree = (input - mf.a2)/(mf.a_m - mf.a2);
    return mf_degree;
  }
  else if (input == mf.a_m)
  {
    return 1;
  }
  else
  {
    return 0;
  }

}

float triangular_mf::get_output_tri_mf_values(triangular_mf mf, float mf_deg ,float &n_val1, float &n_val2) {
  n_val1 = (mf_deg * (mf.a_m - mf.a1)) + mf.a1;
  n_val2 = (mf_deg * (mf.a_m - mf.a2)) + mf.a2;
}

/******************************************************************************/

float trapezoidal_mf::get_trap_mf_value(trapezoidal_mf mf, float input)
{
  float mf_degree; //0 to 1

  if ((mf.a1 <= input) && (input <= mf.a1_1))
  {
    mf_degree = (input - mf.a1)/(mf.a1_1 - mf.a1);
    return mf_degree;
  }
  else if ((mf.a1_1 <= input) && (input <= mf.a2_1))
  {
    return 1;
  }
  else if ((mf.a2_1 <= input) && (input <= mf.a2))
  {
    mf_degree = (input - mf.a2)/(a2_1 - mf.a2);
    return mf_degree;
  }
  else
  {
    return 0;
  }

}

float trapezoidal_mf::get_output_trap_mf_values(trapezoidal_mf mf, float mf_deg, float &n_val1, float &n_val2) {
  n_val1 = (mf_deg * (mf.a1_1 - a1)) + a1;
  n_val2 = (mf_deg * (mf.a2_1 - a2)) + a2;
}

/******************************************************************************/

float half_T_mf::get_half_T_mf_value(half_T_mf mf, float input)
{
  float mf_degree;

  if (input < mf.a1)
  {
    return 0;
  }
  else if ((mf.a1 <= input) && (input <= mf.a2))
  {
    mf_degree = (input - mf.a1)/(mf.a2 - mf.a1);
    return mf_degree;
  }
  else
  {
    return 1;
  }

}

float half_T_mf::get_output_half_T_mf_value(half_T_mf mf, float mf_deg, float &var) {
  var = (mf_deg * (mf.a2 - mf.a1)) + a1;
}


/******************************************************************************/

float L_mf::get_L_mf_value(L_mf mf, float input)
{
  float mf_degree;

  if (input < mf.a1)
  {
    return 1;
  }
  else if ((mf.a1 <= input) && (input <= mf.a2))
  {
    mf_degree = (mf.a2 - input)/(mf.a2 - mf.a1);
    return mf_degree;
  }
  else
  {
    return 0;
  }

}

float L_mf::get_output_L_mf_value(L_mf mf, float mf_deg, float &var) {
  var = mf.a2 - (mf_deg * (mf.a2 - mf.a1));
}
