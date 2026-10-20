#ifndef CMDPARAMETERS_ARGS_H
#define CMDPARAMETERS_ARGS_H
#include "DefAction.h"


/**
 * @brief Container for command line arguments.
 *
 * This does initial parameter check and handles combined parameters. 
 *
 * Combined parameters are multiple single-character parameters
 * combined in a single switch, eg. `-xsh`.
 *
 * Perhaps **TODO:** Enable 'h' for combined parameters of actions.
 */
class Args : public std::vector<std::string> {
public:
  using List = TypedParameter::List;

  Args() = default;
  Args(int argc, char const *argv[]);
  Args(std::string const &w);

  bool check(List const &parameters, DefActions const &actions, bool do_throw = true);
  bool handle(List &parameters);
  bool has_help() const;
  std::string dump();

private:
  bool check(List const &parameters, std::string const &w) const;
  int check(List const &parameters, int current) const;
  Args &operator<<(Args const &rhs);
  bool is_help(std::string const &w) const;
};

#endif // CMDPARAMETERS_ARGS_H
