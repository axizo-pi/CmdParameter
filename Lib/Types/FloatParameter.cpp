#include "FloatParameter.h"
#include <cassert>
#include <string>
#include "Support/Exception.h"
#include "Support/basics.h"
#include "../DefParameter.h"

using std::string;

FloatParameter::FloatParameter(DefParameter &var) : TypedParameter(var, "<float>") {
 	m_defaults.float_value = var.float_default;
}


bool FloatParameter::parse_param_internal(const std::string &in_value) {
  assert(!in_value.empty());

  float value = get_float_value(in_value);

  m_values.float_value = value;
  m_values.m_detected = true;
  return true;
}


std::string FloatParameter::default_indicator() const {
  std::string ret;
 	ret <<  def_param.float_default;
  return ret;
}
