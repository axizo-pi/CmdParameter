#include "../Lib/CmdParameters.h"
#include <iostream>

using namespace std;


CmdParameters params = {
  "Test with simple command line parameters\n",
  {{
    "An integer value",
		{"-i=", "-int="},
    INTEGER,
    "This value can be any integer",
		1
  },{
    "A float value",
		{"-f=", "-float="},
    FLOAT,
    "This value can be any float",
		-1.1f
  }}
};


int main(int argc, const char *argv[]) {
  auto ret = params.handle_commandline(argc, argv, false);
  if (ret != CmdParameters::ALL_IS_WELL) {
    return ret;
  }

  cout << "Switch values: "
	 	 	 << params.parameters()[0]->get_int_value()   << ", "
       << params.parameters()[1]->get_float_value()
			 << endl;

  return 0;
}
