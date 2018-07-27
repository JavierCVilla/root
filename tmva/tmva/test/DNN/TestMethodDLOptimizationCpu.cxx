// @(#)root/tmva/tmva/dnn:$Id$
// Author: Vladimir Ilievski, Ravi Kiran S

/**********************************************************************************
 * Project: TMVA - a Root-integrated toolkit for multivariate data analysis       *
 * Package: TMVA                                                                  *
 * Class  :                                                                       *
 * Web    : http://tmva.sourceforge.net                                           *
 *                                                                                *
 * Description:                                                                   *
 *      Testing MethodDL with DNN for various optimizers ( CPU backend )          *
 *                                                                                *
 * Authors (alphabetical):                                                        *
 *      Ravi Kiran S           <sravikiran0606@gmail.com>  - CERN, Switzerland    *
 *      Vladimir Ilievski      <ilievski.vladimir@live.com>  - CERN, Switzerland  *
 *                                                                                *
 * Copyright (c) 2005-2018:                                                       *
 *      CERN, Switzerland                                                         *
 *      U. of Victoria, Canada                                                    *
 *      MPI-K Heidelberg, Germany                                                 *
 *      U. of Bonn, Germany                                                       *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted according to the terms listed in LICENSE           *
 * (http://tmva.sourceforge.net/LICENSE)                                          *
 **********************************************************************************/

#include "TestMethodDLOptimization.h"
#include "TString.h"

int main()
{
   std::cout << "Testing Method DL for CPU backend: " << std::endl;

   // CPU Architecture:
   TString archCPU = "CPU";

   // SGD Optimizer
   testMethodDL_DNN(archCPU, "SGD");

   return 0;
}
