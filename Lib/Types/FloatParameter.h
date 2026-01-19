#ifndef FLOATPARAMETER_H
#define FLOATPARAMETER_H
#include "TypedParameter.h"


struct FloatParameter : public TypedParameter {
public:
  FloatParameter(DefParameter &var);
  std::string default_indicator() const override; 

private:
  bool parse_param_internal(const std::string &in_value) override;
};

#endif // FLOATPARAMETER_H
