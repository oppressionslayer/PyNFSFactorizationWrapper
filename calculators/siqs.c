//
// This file is part of Alpertron Calculators.
//
// Copyright 2016-2021 Dario Alejandro Alpern
//
// Alpertron Calculators is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Alpertron Calculators is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Alpertron Calculators.  If not, see <http://www.gnu.org/licenses/>.
//
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "bignbr.h"
#include "expression.h"
#include "factor.h"
#include "commonstruc.h"
#if DEBUG_SIQS
#ifndef __EMSCRIPTEN__
#include <stdio.h>
#endif
static int checksum;
#endif
#ifdef __EMSCRIPTEN__
extern char lowerText[MAX_LEN * 16];
extern char *ptrLowerText;
char *ptrSIQSStrings;
int startSieveTenths;
int64_t SIQSModMult;
#endif

#define QUEUE_LENGTH 100

int matrixRows;
int matrixCols;
int smoothsFound;
int totalPartials;
int partialsFound;
int trialDivisions;
uint64_t ValuesSieved;
int congruencesFound;
int polynomialsSieved;
int nbrPartials;
int numberThreads = 1;
extern int NumberLength;
static bool InsertNewRelation(
  const int *rowMatrixB,
  limb *biT, limb *biU, limb *biR,
  int NumberLength);
bool LinearAlgebraPhase(limb* biT, limb* biR, limb* biU, int nbrLength);
#if 0
static unsigned char isProbablePrime(double value);
static int SQUFOF(double N, int queue[]);
#endif
void ShowSIQSStatus(void);
static unsigned int getFactorsOfA(unsigned int seed, int *aindex);
static void sieveThread(BigInteger *result);

#ifdef __EMSCRIPTEN__
static void InitSIQSStrings(int SieveLimit)
{
  char *ptrText = ptrLowerText;  // Point after number that is being factored.
  copyStr(&ptrText, lang ? "<p>Parámetros de SIQS: " : "<p>SIQS parameters: ");
  int2dec(&ptrText, common.siqs.nbrFactorBasePrimes);   // Show number of primes in factor base.
  copyStr(&ptrText, lang ? " primos, límite de la criba: " : " primes, sieve limit: ");
  int2dec(&ptrText, SieveLimit);  // Show sieve limit.
  copyStr(&ptrText, "</p>");
  ptrSIQSStrings = ptrText;
  copyStr(&ptrText, lang ? "<p>Buscando el mejor multiplicador de Knuth-Schroeppel...</p>" :
                         "<p>Searching for Knuth-Schroeppel multiplier...</p>");
  databack(lowerText);
}

// Append multiplier and factor base to SIQS string.
static void getMultAndFactorBase(int multiplier, int FactorBase)
{
  char *ptrText = ptrSIQSStrings;
  copyStr(&ptrText, lang ? "<p>Multiplicador: " : "<p>Multiplier: ");
  int2dec(&ptrText, multiplier);  // Show Knuth-Schroeppel multiplier.
  copyStr(&ptrText, lang ? ", base de factores: " : ", factor base: ");
  int2dec(&ptrText, FactorBase);  // Show factor base.
  copyStr(&ptrText, "</p>");
  ptrSIQSStrings = ptrText;
}

static void ShowSIQSInfo(int timeSieve, int nbrCongruencesFound, int matrixBLength,
  int elapsedTime)
{
  char SIQSInfo[1000];
  float fPercentage = (float)nbrCongruencesFound * 100.0f / (float)matrixBLength;
  int percentage = (int)fPercentage;
  int temp = matrixBLength - nbrCongruencesFound;
  double dU = (double)timeSieve * (double)temp / 
    (double)nbrCongruencesFound;
  int u = (int)dU;
  char *ptrText = SIQSInfo;
  copyStr(&ptrText, "4<p>");
  int2dec(&ptrText, nbrCongruencesFound);  // Show number of congruences found.
  copyStr(&ptrText, lang ? " congruencias halladas (" : " congruences found (");
  int2dec(&ptrText, percentage);  // Show number of congruences found.
  copyStr(&ptrText, lang ? "%) con " : "%) with ");
  int2dec(&ptrText, common.siqs.nbrPrimesUsed);
  copyStr(&ptrText, lang ? " primos diferentes." : " different primes.");
  copyStr(&ptrText, lang ? "<br>Relaciones: " : "<br>Relations: ");
  int2dec(&ptrText, smoothsFound);   // Show number of full congruences.
  copyStr(&ptrText, lang ? " completas y " : " full and ");
  int2dec(&ptrText, partialsFound);  // Show number of built congruences.
  copyStr(&ptrText, lang ? " obtenidas de " : " found from ");
  int2dec(&ptrText, totalPartials);  // Show number of partial congruences.
  copyStr(&ptrText, lang ? " parciales." : " partials.");
  copyStr(&ptrText, "<br><br><progress value=\"");
  int2dec(&ptrText, percentage);
  copyStr(&ptrText, "\" max=\"100\"></progress><br>");
  GetDHMS(&ptrText, elapsedTime);
  if ((timeSieve > 1) && (nbrCongruencesFound > 10))
  {
    copyStr(&ptrText, "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
    copyStr(&ptrText, lang ? " Fin de la criba en " : " End sieve in ");
    GetDHMS(&ptrText, u / 2);
  }
  copyStr(&ptrText, "</p>");
  databack(SIQSInfo);
}

#endif

#if DEBUG_SIQS
void ShowSquareModP(char* pOutput)
{
  char* ptrOutput = pOutput;
  copyStr(&ptrOutput, "^2, p)");
#if DEBUG_SIQS == 3
  copyStr(&ptrOutput, " checksum = ");
  int2dec(&ptrOutput, checksum);
#endif
  *ptrOutput = 0;
  ptrOutput = output;
#ifdef __EMSCRIPTEN__
  ptrOutput++;    // Skip character '9' (command to send data to console).
#endif
  while (*ptrOutput != '\0')
  {
    checksum += (unsigned char)*ptrOutput;
    ptrOutput++;
  }
#ifdef __EMSCRIPTEN__
  databack(output);
#else
  printf("%s\n", output);
#endif
}
#endif

static void PerformSiqsSieveStage(PrimeSieveData *primeSieveData,
  short *SieveArray,
  int PolynomialIndex,
  limb *biLinearCoeff,
  int nbrLength)
{
  short logPrimeEvenPoly;
  short logPrimeOddPoly;
  int currentPrime;
  int F1;
  int F2;
  int F3;
  int F4;
  int X1;
  int X2;
  short* ptrSieveArray;
  const short* ptrEndSieveArray1;
  const short* ptrEndSieveArray2;
  int index;
  int index2;
  int indexFactorA;
  int mask;
  bool polyadd;
  int S1;
  int G0;
  int G1;
  int G2;
  int G3;
  int H0;
  int H1;
  int H2;
  int H3;
  int I0;
  int I1;
  int I2;
  int I3;
  int nbrBytes;
  const int* ptrBainv2;
  PrimeSieveData *rowPrimeSieveData;

  F1 = PolynomialIndex; // PolynomialIndex is even.
  indexFactorA = 0;     // Get lowest bit set of PolynomialIndex.
  do
  {
    F1 >>= 1;
    indexFactorA++;
  } while ((F1 & 1) == 0);
  rowPrimeSieveData = primeSieveData + 1;
  ptrBainv2 = &common.siqs.Bainv2[indexFactorA][1];
  polyadd = (F1 & 2) != 0; // Get next bit to lowest bit set of PolynomialIndex.
  if (polyadd)   // Adjust value of B as appropriate according to Gray code.
  {
    AddBigNbrB(biLinearCoeff, common.siqs.biLinearDelta[indexFactorA], biLinearCoeff,
      nbrLength);
    AddBigNbrB(biLinearCoeff, common.siqs.biLinearDelta[indexFactorA], biLinearCoeff,
      nbrLength);
    F1 = -*(ptrBainv2 - PRIME_SIEVE_DATA_LEN);
  }
  else
  {
    SubtractBigNbrB(biLinearCoeff, common.siqs.biLinearDelta[indexFactorA],
      biLinearCoeff, nbrLength);
    SubtractBigNbrB(biLinearCoeff, common.siqs.biLinearDelta[indexFactorA],
      biLinearCoeff, nbrLength);
    F1 = *(ptrBainv2 - PRIME_SIEVE_DATA_LEN);
  }
  indexFactorA--;
  X1 = common.siqs.SieveLimit * 2;
  ptrEndSieveArray1 = &SieveArray[X1];
  int temp = (int)common.siqs.logar2 - (int)common.siqs.threshold;
  if (((rowPrimeSieveData->soln1 += F1) & 1) == 0)
  {
    *(SieveArray + 0) = (short)temp;
    *(SieveArray + 1) = (short)(-common.siqs.threshold);
  }
  else
  {
    *(SieveArray + 0) = (short)(-common.siqs.threshold);
    *(SieveArray + 1) = (short)temp;
  }
  temp *= 256;
  if (((rowPrimeSieveData->soln1 + rowPrimeSieveData->Bainv2_0) & 1) == 0)
  {
    *(SieveArray + 0) += (short)temp;
    *(SieveArray + 1) += (short)((-common.siqs.threshold) * 256);
  }
  else
  {
    *(SieveArray + 0) += (short)((-common.siqs.threshold) * 256);
    *(SieveArray + 1) += (short)temp;
  }
  F2 = 2;
  index = 2;
  rowPrimeSieveData = primeSieveData + 2;
  ptrBainv2 = &common.siqs.Bainv2[indexFactorA][2];
  for (;;)
  {
    currentPrime = rowPrimeSieveData->value;
    F3 = F2 * currentPrime;
    F3 += ((X1 + 1 - F3) >> 31) & (X1 + 1 - F3);  // Min of F3 and X1 + 1.
    F4 = F2;
    while ((F4 * 2) <= F3)
    {
      nbrBytes = F4 * (int)sizeof(*SieveArray);
      (void)memcpy(SieveArray + F4, SieveArray, nbrBytes);
      F4 *= 2;
    }
    nbrBytes = (F3 - F4) * (int)sizeof(*SieveArray);
    (void)memcpy(SieveArray + F4, SieveArray, nbrBytes);
    if (F3 == (X1 + 1))
    {
      break;
    }
    F1 = currentPrime;
    logPrimeEvenPoly = 1;
    while (F1 >= 5)
    {
      F1 /= 3;
      logPrimeEvenPoly++;
    }
    logPrimeOddPoly = (short)(logPrimeEvenPoly * 256);
    F1 = (polyadd ? -*ptrBainv2 : *ptrBainv2);
    index2 = (rowPrimeSieveData->soln1 + F1) % currentPrime;
    index2 += currentPrime & (index2 >> 31);
    rowPrimeSieveData->soln1 = index2;
    for (; index2 < F3; index2 += currentPrime)
    {
      *(SieveArray + index2) += logPrimeEvenPoly;
    }
    for (index2 = (rowPrimeSieveData->soln1 + currentPrime -
      rowPrimeSieveData->Bainv2_0) % currentPrime;
      index2 < F3;
      index2 += currentPrime)
    {
      *(SieveArray + index2) += logPrimeOddPoly;
    }
    if (currentPrime != common.siqs.multiplier)
    {
      F1 = (rowPrimeSieveData->soln1 + currentPrime -
        rowPrimeSieveData->difsoln) % currentPrime;
      for (index2 = F1; index2 < F3; index2 += currentPrime)
      {
        *(SieveArray + index2) += logPrimeEvenPoly;
      }
      for (index2 = (F1 + currentPrime -
        rowPrimeSieveData->Bainv2_0) % currentPrime;
        index2 < F3;
        index2 += currentPrime)
      {
        *(SieveArray + index2) += logPrimeOddPoly;
      }
    }
    index++;
    rowPrimeSieveData++;
    ptrBainv2++;
    F2 *= currentPrime;
  }

  F1 = (primeSieveData + common.siqs.smallPrimeUpperLimit) -> value;
  logPrimeEvenPoly = 1;
  logPrimeOddPoly = 0x100;
  mask = 5;
  while (F1 >= 5)
  {
    F1 /= 3;
    logPrimeEvenPoly++;
    logPrimeOddPoly += 0x100;
    mask *= 3;
  }
  if (polyadd)
  {
    for (; index < common.siqs.smallPrimeUpperLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      S1 = rowPrimeSieveData->soln1 - *ptrBainv2;
      S1 += (S1 >> 31) & currentPrime;
      rowPrimeSieveData->soln1 = S1;
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (index = common.siqs.smallPrimeUpperLimit; index < common.siqs.firstLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = currentPrime + currentPrime;
      F3 = F2 + currentPrime;
      F4 = F3 + currentPrime;
      S1 = rowPrimeSieveData->soln1 - *ptrBainv2;
      S1 += (S1 >> 31) & currentPrime;
      rowPrimeSieveData->soln1 = S1;
      index2 = ((X1 / F4) * F4) + S1;
      G0 = -rowPrimeSieveData->difsoln;
      // If (S1 + G0) < 0, G0 += currentPrime.
      G0 += ((S1 + G0) >> 31) & currentPrime;
      G1 = G0 + currentPrime;
      G2 = G1 + currentPrime;
      G3 = G2 + currentPrime;
      H0 = -rowPrimeSieveData->Bainv2_0;
      // If (S1 + H0) < 0, H0 += currentPrime.
      H0 += ((S1 + H0) >> 31) & currentPrime;
      H1 = H0 + currentPrime;
      H2 = H1 + currentPrime;
      H3 = H2 + currentPrime;
      I0 = H0 - rowPrimeSieveData->difsoln;
      // If (S1 + I0) < 0, I0 += currentPrime.
      I0 += ((S1 + I0) >> 31) & currentPrime;
      I1 = I0 + currentPrime;
      I2 = I1 + currentPrime;
      I3 = I2 + currentPrime;
      ptrSieveArray = &SieveArray[index2];
      do
      {
        *ptrSieveArray += logPrimeEvenPoly;
        *(ptrSieveArray + currentPrime) += logPrimeEvenPoly;
        *(ptrSieveArray + F2) += logPrimeEvenPoly;
        *(ptrSieveArray + F3) += logPrimeEvenPoly;
        *(ptrSieveArray + G0) += logPrimeEvenPoly;
        *(ptrSieveArray + G1) += logPrimeEvenPoly;
        *(ptrSieveArray + G2) += logPrimeEvenPoly;
        *(ptrSieveArray + G3) += logPrimeEvenPoly;
        *(ptrSieveArray + H0) += logPrimeOddPoly;
        *(ptrSieveArray + H1) += logPrimeOddPoly;
        *(ptrSieveArray + H2) += logPrimeOddPoly;
        *(ptrSieveArray + H3) += logPrimeOddPoly;
        *(ptrSieveArray + I0) += logPrimeOddPoly;
        *(ptrSieveArray + I1) += logPrimeOddPoly;
        *(ptrSieveArray + I2) += logPrimeOddPoly;
        *(ptrSieveArray + I3) += logPrimeOddPoly;
        ptrSieveArray -= F4;
      } while (ptrSieveArray >= &SieveArray[0]);
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.secondLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      F2 = currentPrime + currentPrime;
      F3 = F2 + currentPrime;
      F4 = F2 + F2;
      X2 = X1 - F4;
      ptrEndSieveArray2 = &SieveArray[X2];
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      if (rowPrimeSieveData->difsoln >= 0)
      {
        F1 = rowPrimeSieveData->soln1 - *ptrBainv2;
        F1 += (F1 >> 31) & currentPrime;
        rowPrimeSieveData->soln1 = F1;
        ptrSieveArray = &SieveArray[F1];
        do
        {
          *ptrSieveArray += logPrimeEvenPoly;
          *(ptrSieveArray + currentPrime) += logPrimeEvenPoly;
          *(ptrSieveArray + F2) += logPrimeEvenPoly;
          *(ptrSieveArray + F3) += logPrimeEvenPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeEvenPoly;
        }
        index2 = F1 - rowPrimeSieveData->Bainv2_0;
        index2 += (index2 >> 31) & currentPrime;
        ptrSieveArray = &SieveArray[index2];
        do
        {
          *ptrSieveArray += logPrimeOddPoly;
          *(ptrSieveArray + currentPrime) += logPrimeOddPoly;
          *(ptrSieveArray + F2) += logPrimeOddPoly;
          *(ptrSieveArray + F3) += logPrimeOddPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeOddPoly;
        }
        F1 -= rowPrimeSieveData->difsoln;
        F1 += (F1 >> 31) & currentPrime;
        ptrSieveArray = &SieveArray[F1];
        do
        {
          *ptrSieveArray += logPrimeEvenPoly;
          *(ptrSieveArray + currentPrime) += logPrimeEvenPoly;
          *(ptrSieveArray + F2) += logPrimeEvenPoly;
          *(ptrSieveArray + F3) += logPrimeEvenPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeEvenPoly;
        }
        index2 = F1 - rowPrimeSieveData->Bainv2_0;
        index2 += (index2 >> 31) & currentPrime;
        ptrSieveArray = &SieveArray[index2];
        do
        {
          *ptrSieveArray += logPrimeOddPoly;
          *(ptrSieveArray + currentPrime) += logPrimeOddPoly;
          *(ptrSieveArray + F2) += logPrimeOddPoly;
          *(ptrSieveArray + F3) += logPrimeOddPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeOddPoly;
        }
      }
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.thirdLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = rowPrimeSieveData->soln1 - *ptrBainv2;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      ptrSieveArray = &SieveArray[F2];
      do
      {
        *ptrSieveArray += logPrimeEvenPoly;
        ptrSieveArray += currentPrime;
      } while (ptrSieveArray <= ptrEndSieveArray1);
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      do
      {
        *(SieveArray + F1) += logPrimeOddPoly;
        F1 += currentPrime;
      } while (F1 <= X1);
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      ptrSieveArray = &SieveArray[F2];
      do
      {
        *ptrSieveArray += logPrimeEvenPoly;
        ptrSieveArray += currentPrime;
      } while (ptrSieveArray <= ptrEndSieveArray1);
      F2 += (currentPrime & ((F2 - F3) >> 31)) - F3;
      do
      {
        *(SieveArray + F2) += logPrimeOddPoly;
      } while ((F2 += currentPrime) <= X1);
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.nbrPrimes2; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = rowPrimeSieveData->soln1 - *ptrBainv2;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 += (currentPrime & ((F2 - F3) >> 31)) - F3;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
      index++;
      currentPrime = rowPrimeSieveData->value;
      F2 = rowPrimeSieveData->soln1 - *ptrBainv2;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 += (currentPrime & ((F2 - F3) >> 31)) - F3;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
      index++;
      currentPrime = rowPrimeSieveData->value;
      F2 = rowPrimeSieveData->soln1 - *ptrBainv2;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
      index++;
      currentPrime = rowPrimeSieveData->value;
      F2 = rowPrimeSieveData->soln1 - *ptrBainv2;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.nbrFactorBasePrimes; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = rowPrimeSieveData->soln1 - *ptrBainv2;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
    }
  }
  else
  {
    for (; index < common.siqs.smallPrimeUpperLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      S1 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      S1 += currentPrime & (S1 >> 31);
      rowPrimeSieveData->soln1 = S1;
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (index = common.siqs.smallPrimeUpperLimit; index < common.siqs.firstLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = currentPrime + currentPrime;
      F3 = F2 + currentPrime;
      F4 = F3 + currentPrime;
      S1 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      S1 += (S1 >> 31) & currentPrime;
      rowPrimeSieveData->soln1 = S1;
      index2 = ((X1 / F4) * F4) + S1;
      G0 = -rowPrimeSieveData->difsoln;
      // If (S1 + G0) < 0, G0 += currentPrime.
      G0 += ((S1 + G0) >> 31) & currentPrime;
      G1 = G0 + currentPrime;
      G2 = G1 + currentPrime;
      G3 = G2 + currentPrime;
      H0 = -rowPrimeSieveData->Bainv2_0;
      // If (S1 + H0) < 0, H0 += currentPrime.
      H0 += ((S1 + H0) >> 31) & currentPrime;
      H1 = H0 + currentPrime;
      H2 = H1 + currentPrime;
      H3 = H2 + currentPrime;
      I0 = H0 - rowPrimeSieveData->difsoln;
      // If (S1 + I0) < 0, I0 += currentPrime.
      I0 += ((S1 + I0) >> 31) & currentPrime;
      I1 = I0 + currentPrime;
      I2 = I1 + currentPrime;
      I3 = I2 + currentPrime;
      ptrSieveArray = &SieveArray[index2];
      do
      {
        *ptrSieveArray += logPrimeEvenPoly;
        *(ptrSieveArray + currentPrime) += logPrimeEvenPoly;
        *(ptrSieveArray + F2) += logPrimeEvenPoly;
        *(ptrSieveArray + F3) += logPrimeEvenPoly;
        *(ptrSieveArray + G0) += logPrimeEvenPoly;
        *(ptrSieveArray + G1) += logPrimeEvenPoly;
        *(ptrSieveArray + G2) += logPrimeEvenPoly;
        *(ptrSieveArray + G3) += logPrimeEvenPoly;
        *(ptrSieveArray + H0) += logPrimeOddPoly;
        *(ptrSieveArray + H1) += logPrimeOddPoly;
        *(ptrSieveArray + H2) += logPrimeOddPoly;
        *(ptrSieveArray + H3) += logPrimeOddPoly;
        *(ptrSieveArray + I0) += logPrimeOddPoly;
        *(ptrSieveArray + I1) += logPrimeOddPoly;
        *(ptrSieveArray + I2) += logPrimeOddPoly;
        *(ptrSieveArray + I3) += logPrimeOddPoly;
        ptrSieveArray -= F4;
      } while (ptrSieveArray >= &SieveArray[0]);
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.secondLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      F2 = currentPrime + currentPrime;
      F3 = F2 + currentPrime;
      F4 = F2 + F2;
      X2 = X1 - F4;
      ptrEndSieveArray2 = &SieveArray[X2];
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      if (rowPrimeSieveData->difsoln >= 0)
      {
        F1 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
        F1 += currentPrime & (F1 >> 31);
        rowPrimeSieveData->soln1 = F1;
        ptrSieveArray = &SieveArray[F1];
        do
        {
          *ptrSieveArray += logPrimeEvenPoly;
          *(ptrSieveArray + currentPrime) += logPrimeEvenPoly;
          *(ptrSieveArray + F2) += logPrimeEvenPoly;
          *(ptrSieveArray + F3) += logPrimeEvenPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeEvenPoly;
        }
        index2 = F1 - rowPrimeSieveData->Bainv2_0;
        index2 += (index2 >> 31) & currentPrime;
        ptrSieveArray = &SieveArray[index2];
        do
        {
          *ptrSieveArray += logPrimeOddPoly;
          *(ptrSieveArray + currentPrime) += logPrimeOddPoly;
          *(ptrSieveArray + F2) += logPrimeOddPoly;
          *(ptrSieveArray + F3) += logPrimeOddPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeOddPoly;
        }
        F1 -= rowPrimeSieveData->difsoln;
        F1 += (F1 >> 31) & currentPrime;
        ptrSieveArray = &SieveArray[F1];
        do
        {
          *ptrSieveArray += logPrimeEvenPoly;
          *(ptrSieveArray + currentPrime) += logPrimeEvenPoly;
          *(ptrSieveArray + F2) += logPrimeEvenPoly;
          *(ptrSieveArray + F3) += logPrimeEvenPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeEvenPoly;
        }
        index2 = F1 - rowPrimeSieveData->Bainv2_0;
        index2 += (index2 >> 31) & currentPrime;
        ptrSieveArray = &SieveArray[index2];
        do
        {
          *ptrSieveArray += logPrimeOddPoly;
          *(ptrSieveArray + currentPrime) += logPrimeOddPoly;
          *(ptrSieveArray + F2) += logPrimeOddPoly;
          *(ptrSieveArray + F3) += logPrimeOddPoly;
          ptrSieveArray += F4;
        } while (ptrSieveArray <= ptrEndSieveArray2);
        for (; ptrSieveArray <= ptrEndSieveArray1; ptrSieveArray += currentPrime)
        {
          *ptrSieveArray += logPrimeOddPoly;
        }
      }
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.thirdLimit; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      index2 = F2;
      do
      {
        *(SieveArray + index2) += logPrimeEvenPoly;
      } while ((index2 += currentPrime) <= X1);
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      do
      {
        *(SieveArray + F1) += logPrimeOddPoly;
      } while ((F1 += currentPrime) <= X1);
      F2 -= rowPrimeSieveData->difsoln;
      F1 = F2 += currentPrime & (F2 >> 31);
      do
      {
        *(SieveArray + F2) += logPrimeEvenPoly;
      } while ((F2 += currentPrime) <= X1);
      F1 -= F3;
      F1 += currentPrime & (F1 >> 31);
      do
      {
        *(SieveArray + F1) += logPrimeOddPoly;
      } while ((F1 += currentPrime) <= X1);
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.nbrPrimes2; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
      index++;
      currentPrime = rowPrimeSieveData->value;
      F2 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
      index++;
      currentPrime = rowPrimeSieveData->value;
      F2 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
      index++;
      currentPrime = rowPrimeSieveData->value;
      F2 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
    }
    for (; index < common.siqs.nbrFactorBasePrimes; index++)
    {
      currentPrime = rowPrimeSieveData->value;
      if (currentPrime >= mask)
      {
        mask *= 3;
        logPrimeEvenPoly++;
        logPrimeOddPoly += 0x100;
      }
      F2 = rowPrimeSieveData->soln1 + *ptrBainv2 - currentPrime;
      F2 += currentPrime & (F2 >> 31);
      rowPrimeSieveData->soln1 = F2;
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F3 = rowPrimeSieveData->Bainv2_0;
      F1 = F2 - F3;
      F1 += currentPrime & (F1 >> 31);
      *(SieveArray + (((F1 - X1) >> 31) & F1)) += logPrimeOddPoly;
      F2 -= rowPrimeSieveData->difsoln;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeEvenPoly;
      F2 -= F3;
      F2 += currentPrime & (F2 >> 31);
      *(SieveArray + (((F2 - X1) >> 31) & F2)) += logPrimeOddPoly;
      rowPrimeSieveData++;
      ptrBainv2++;
    }
  }
}

static int getIndexFromDivisor(double dDivid)
{
  int left = -1;
  int right = common.siqs.nbrFactorBasePrimes;
  int median = right;
  int divid = (int)dDivid;
  while (left != right)
  {
    int nbr;
    median = ((right - left) >> 1) + left;
    nbr = common.siqs.primeTrialDivisionData[median].value;
    if (nbr < divid)
    {
      if ((median == left) &&
        (congruencesFound >= common.siqs.matrixBLength))
      {
        return 0;
      }
      left = median;
    }
    else if (nbr > divid)
    {
      right = median;
    }
    else
    {
      break;
    }
  }
  return median;
}

static int PerformTrialDivision(const PrimeSieveData *primeSieveData,
  int rowMatrixBbeforeMerge[],
  int index2,
  const limb *biDividend, int rowSquares[],
  int NumberLengthDividend,
  bool oddPolynomial)
{
  int biR0 = 0;
  int biR1 = 0;  
  int biR2 = 0;
  int biR3 = 0;
  int biR4 = 0;
  int biR5 = 0;
  int biR6 = 0;
  int nbrSquares = rowSquares[0];
  int Divisor;
  int divis;
  int expParity;
  int index;
  int nbrColumns = rowMatrixBbeforeMerge[0];
  const PrimeSieveData *rowPrimeSieveData;
  const PrimeTrialDivisionData *rowPrimeTrialDivisionData;
  switch (NumberLengthDividend)
  {
  case 7:
    biR6 = biDividend[6].x;
    /* no break */
  case 6:
    biR5 = biDividend[5].x;
    /* no break */
  case 5:
    biR4 = biDividend[4].x;
    /* no break */
  case 4:
    biR3 = biDividend[3].x;
    /* no break */
  case 3:
    biR2 = biDividend[2].x;
    /* no break */
  case 1:
  case 2:
    biR1 = biDividend[1].x;
    biR0 = biDividend[0].x;
    break;
  default:
    break;
  }
  expParity = 0;
  if (NumberLengthDividend <= 1)
  {
    rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[1];
    for (index = 1; index < common.siqs.nbrFactorBasePrimes; index++)
    {
      Divisor = rowPrimeTrialDivisionData->value;
      if (biR0 != 0)
      {
        while ((biR0 % Divisor) == 0)
        {
          biR0 /= Divisor;
          expParity = 1 - expParity;
          if (expParity == 0)
          {
            rowSquares[nbrSquares] = Divisor;
            nbrSquares++;
          }
        }
        if (expParity != 0)
        {
          rowMatrixBbeforeMerge[nbrColumns] = index;
          nbrColumns++;
          expParity = 0;
        }
        rowPrimeTrialDivisionData++;
      }
    }
  }
  else
  {   // Dividend has at least two limbs.
    bool mostSignificantLimbZero = false;
    double dRem;
    double dCurrentPrime;
    double dDivid;
    double dLimbMult;
    double dQuot;
    int Dividend;
    int nbr;
    int iRem;
    int left;
    int right;
    int median;
    bool fullRemainder;
    int indexFactorA = 0;
    int newFactorAIndex;
    bool testFactorA = true;
    newFactorAIndex = common.siqs.aindex[0];
    rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[1];
    index = 1;
    while (testFactorA)
    {
      fullRemainder = false;
      if (index < 3)
      {
        fullRemainder = true;
      }
      else if (index == newFactorAIndex)
      {
        fullRemainder = true;
        indexFactorA++;
        if (indexFactorA == common.siqs.nbrFactorsA)
        {
          testFactorA = false;   // All factors of A were tested.
        }
        else
        {
          newFactorAIndex = common.siqs.aindex[indexFactorA];
        }
      }
      else
      {     // Nothing to do.
      }
      for (;;)
      {
        if (fullRemainder == false)
        {
          rowPrimeSieveData = primeSieveData + index;
          Divisor = rowPrimeSieveData->value;
          divis = Divisor;
          if (oddPolynomial)
          {
            iRem = index2 - rowPrimeSieveData->soln1 +
              rowPrimeSieveData->Bainv2_0;
          }
          else
          {
            iRem = index2 - rowPrimeSieveData->soln1;
          }
          iRem += ((iRem >> 31) & divis) - divis;
          iRem += (iRem >> 31) & divis;
          if (iRem >= divis)
          {
            iRem %= divis;
          }
          if ((iRem != 0) && (iRem != (divis - rowPrimeSieveData->difsoln)))
          {
            if (expParity != 0)
            {
              rowMatrixBbeforeMerge[nbrColumns] = index;
              nbrColumns++;
              expParity = 0;
            }
            break;              // Process next prime.
          }
          fullRemainder = true;
        }
        Divisor = rowPrimeTrialDivisionData->value;
        divis = Divisor;
        switch (NumberLengthDividend)
        {
        case 7:
          dRem = (double)biR6*(double)rowPrimeTrialDivisionData->exp6 +
            (double)biR5*(double)rowPrimeTrialDivisionData->exp5 +
            (double)biR4*(double)rowPrimeTrialDivisionData->exp4 +
            (double)biR3*(double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2*(double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1*(double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 6:
          dRem = (double)biR5*(double)rowPrimeTrialDivisionData->exp5 +
            (double)biR4*(double)rowPrimeTrialDivisionData->exp4 +
            (double)biR3*(double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2*(double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1*(double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 5:
          dRem = (double)biR4*(double)rowPrimeTrialDivisionData->exp4 +
            (double)biR3*(double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2*(double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1*(double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 4:
          dRem = (double)biR3*(double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2*(double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1*(double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 3:
          dRem = (double)biR2*(double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1*(double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        default:
          dRem = (double)biR1*(double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        }
        dCurrentPrime = (double)divis;
        if (dRem != floor(dRem / dCurrentPrime)*dCurrentPrime)
        {                     // Number is not a multiple of prime.
          if (expParity != 0)
          {
            rowMatrixBbeforeMerge[nbrColumns] = index;
            nbrColumns++;
            expParity = 0;
          }
          break;              // Process next prime.
        }
        expParity = 1 - expParity;
        if (expParity == 0)
        {
          rowSquares[nbrSquares] = Divisor;
          nbrSquares++;
        }
        dRem = 0;
        // Perform division
        dLimbMult = (double)(1U << BITS_PER_GROUP);
        dCurrentPrime = (double)divis;
        switch (NumberLengthDividend)
        {
        case 7:     // {biR6 - biR0} <- {biR6 - biR0} / divis
          Dividend = biR6;
          biR6 = Dividend / Divisor;
          dRem = (double)Dividend - ((double)biR6 * Divisor);
          /* no break */
        case 6:     // {biR5 - biR0} <- {biR5 - biR0} / divis
          dDivid = (double)biR5 + (dRem*dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR5 = (int)dQuot;
          /* no break */
        case 5:     // {biR4 - biR0} <- {biR4 - biR0} / divis
          dDivid = (double)biR4 + (dRem*dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR4 = (int)dQuot;
          /* no break */
        case 4:     // {biR3 - biR0} <- {biR3 - biR0} / divis
          dDivid = (double)biR3 + (dRem*dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR3 = (int)dQuot;
          /* no break */
        case 3:     // {biR2 - biR0} <- {biR2 - biR0} / divis
          dDivid = (double)biR2 + (dRem*dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR2 = (int)dQuot;
          /* no break */
        case 2:     // {biR1 - biR0} <- {biR1 - biR0} / divis
          dDivid = (double)biR1 + (dRem*dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR1 = (int)dQuot;
          dDivid = (double)biR0 + (dRem*dLimbMult);
          biR0 = (int)floor(dDivid / dCurrentPrime);
          break;
        default:
          break;
        }
        switch (NumberLengthDividend)
        {
        case 7:
          if (biR6 == 0)
          {
            mostSignificantLimbZero = 6;
          }
          break;
        case 6:
          if (biR5 == 0)
          {
            mostSignificantLimbZero = 5;
          }
          break;
        case 5:
          if (biR4 == 0)
          {
            mostSignificantLimbZero = 4;
          }
          break;
        case 4:
          if (biR3 == 0)
          {
            mostSignificantLimbZero = 3;
          }
          break;
        case 3:
          if (biR2 == 0)
          {
            mostSignificantLimbZero = 2;
          }
          break;
        case 2:    // Criteria is to fit in a double (52 bits).
          if (biR1 == 0)
          {
            mostSignificantLimbZero = 1;
          }
          break;
        default:
          break;
        }
        if ((NumberLengthDividend <= 2) && (biR1 < (1 << (52 - BITS_PER_GROUP))))
        {       // Number fits in a double.
          double dDividend = (double)biR1 * (double)(1U << BITS_PER_GROUP) + (double)biR0;
          int sqrtDivid = (int)(floor(sqrt(dDividend)));
          rowPrimeSieveData = primeSieveData + index;
          for (; index < common.siqs.nbrFactorBasePrimes; index++)
          {
            Divisor = rowPrimeSieveData->value;
            if (testFactorA && (index == newFactorAIndex))
            {
              indexFactorA++;
              if (indexFactorA == common.siqs.nbrFactorsA)
              {
                testFactorA = false;   // All factors of A were tested.
              }
              else
              {
                newFactorAIndex = common.siqs.aindex[indexFactorA];
              }
            }
            while (dDividend == floor(dDividend / (double)Divisor) * (double)Divisor)
            {        // dDivid is multiple of Divisor.
              dDividend /= Divisor;
              sqrtDivid = (int)(floor(sqrt(dDividend))) + 1;
              expParity = 1 - expParity;
              if (expParity == 0)
              {
                rowSquares[nbrSquares] = Divisor;
                nbrSquares++;
              }
            }
            if (expParity != 0)
            {
              rowMatrixBbeforeMerge[nbrColumns] = index;
              nbrColumns++;
              expParity = 0;
            }
#if 0
            if (dDivid < (double)common.siqs.largePrimeUpperBound *
              (double)upperBound)
            {         // Use SQUFOF for finding factors.
              int factor, bigfactor = 1;
              while (!isProbablePrime(dDivid))
              {
                int factor = SQUFOF((double)dDivid, queue);
                if (factor == 0)
                {
                  return 0;  // Go out if factor could not be found.
                }
                if (factor > upperBound)
                {
                  if ((factor > common.siqs.largePrimeUpperBound) || (bigfactor > 1))
                  {
                    return 0;   // Discard relation.
                  }
                  bigfactor = factor;
                }
                else
                {
                  index = getIndexFromDivisor(factor);
                  if (rowMatrixBbeforeMerge[nbrColumns] == index)
                  {
                    nbrColumns--;
                    rowSquares[nbrSquares] = factor;
                    nbrSquares++;
                  }
                  else
                  {
                    rowMatrixBbeforeMerge[nbrColumns] = index;
                    nbrColumns++;
                  }
                }
                dDivid /= factor;
              }
              factor = (int)dDivid;
              if (factor > upperBound)
              {
                if ((factor > common.siqs.largePrimeUpperBound) || (bigfactor > 1))
                {
                  return 0;   // Discard relation.
                }
                bigfactor = factor;
              }
              else
              {
                index = getIndexFromDivisor(factor);
                if (rowMatrixBbeforeMerge[nbrColumns] == index)
                {
                  nbrColumns--;
                  rowSquares[nbrSquares] = factor;
                  nbrSquares++;
                }
                else
                {
                  rowMatrixBbeforeMerge[nbrColumns] = index;
                  nbrColumns++;
                }
              }
              rowMatrixBbeforeMerge[0] = nbrColumns;
              return bigfactor;
            }
#endif
            if (Divisor > sqrtDivid)
            {                     // End of trial division.
              int dividend;
              if (dDividend > (double)MAX_INT_NBR)
              {
                return 0;  // Discard this congruence because of large cofactor.
              }
              dividend = (int)dDividend;
              rowSquares[0] = nbrSquares;
              index = common.siqs.nbrFactorBasePrimes - 1;
              if ((dividend <= common.siqs.primeTrialDivisionData[index].value) &&
                (dividend > 1))
              {          // Perform binary search to find the index.
                index = getIndexFromDivisor(dDividend);
                rowMatrixBbeforeMerge[nbrColumns] = index;
                nbrColumns++;
                rowMatrixBbeforeMerge[0] = nbrColumns;
                return 1;
              }
              rowMatrixBbeforeMerge[0] = nbrColumns;
              return (int)dDividend;
            }
            rowPrimeSieveData++;
          }
          break;
        }
      }             /* end while */
      rowPrimeTrialDivisionData++;
      index++;
    }              /* end while */
    int nbrFactorBasePrimes = common.siqs.nbrFactorBasePrimes;
    rowPrimeSieveData = primeSieveData + index;
    rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[index];
    while (index < nbrFactorBasePrimes)
    {
      fullRemainder = false;
      for (;;)
      {
        if (fullRemainder == false)
        {
          Divisor = rowPrimeSieveData->value;
          divis = Divisor;
          if (oddPolynomial)
          {
            iRem = index2 - rowPrimeSieveData->soln1 +
              rowPrimeSieveData->Bainv2_0;
          }
          else
          {
            iRem = index2 - rowPrimeSieveData->soln1;
          }
          iRem += ((iRem >> 31) & divis) - divis;
          iRem += (iRem >> 31) & divis;
          if (iRem >= divis)
          {
            iRem %= divis;
          }
          if ((iRem != 0) && (iRem != (divis - rowPrimeSieveData->difsoln)))
          {
            if (expParity != 0)
            {
              rowMatrixBbeforeMerge[nbrColumns] = index;
              nbrColumns++;
              expParity = 0;
            }
            break;              // Process next prime.
          }
          fullRemainder = true;
        }
        Divisor = rowPrimeTrialDivisionData->value;
        divis = Divisor;
        switch (NumberLengthDividend)
        {
        case 7:
          dRem = (double)biR6 * (double)rowPrimeTrialDivisionData->exp6 +
            (double)biR5 * (double)rowPrimeTrialDivisionData->exp5 +
            (double)biR4 * (double)rowPrimeTrialDivisionData->exp4 +
            (double)biR3 * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2 * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1 * (double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 6:
          dRem = (double)biR5 * (double)rowPrimeTrialDivisionData->exp5 +
            (double)biR4 * (double)rowPrimeTrialDivisionData->exp4 +
            (double)biR3 * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2 * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1 * (double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 5:
          dRem = (double)biR4 * (double)rowPrimeTrialDivisionData->exp4 +
            (double)biR3 * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2 * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1 * (double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 4:
          dRem = (double)biR3 * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biR2 * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1 * (double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        case 3:
          dRem = (double)biR2 * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biR1 * (double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        default:
          dRem = (double)biR1 * (double)rowPrimeTrialDivisionData->exp1 + biR0;
          break;
        }
        dCurrentPrime = (double)divis;
        if (dRem != floor(dRem / dCurrentPrime) * dCurrentPrime)
        {                     // Number is not a multiple of prime.
          if (expParity != 0)
          {
            rowMatrixBbeforeMerge[nbrColumns] = index;
            nbrColumns++;
            expParity = 0;
          }
          break;              // Process next prime.
        }
        expParity = 1 - expParity;
        if (expParity == 0)
        {
          rowSquares[nbrSquares] = Divisor;
          nbrSquares++;
        }
        dRem = 0;
        // Perform division
        dLimbMult = (double)(1U << BITS_PER_GROUP);
        dCurrentPrime = (double)divis;
        switch (NumberLengthDividend)
        {
        case 7:     // {biR6 - biR0} <- {biR6 - biR0} / divis
          Dividend = biR6;
          biR6 = Dividend / Divisor;
          dRem = (double)Dividend - ((double)biR6 * (double)Divisor);
          /* no break */
        case 6:     // {biR5 - biR0} <- {biR5 - biR0} / divis
          dDivid = (double)biR5 + (dRem * dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR5 = (int)dQuot;
          /* no break */
        case 5:     // {biR4 - biR0} <- {biR4 - biR0} / divis
          dDivid = (double)biR4 + (dRem * dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR4 = (int)dQuot;
          /* no break */
        case 4:     // {biR3 - biR0} <- {biR3 - biR0} / divis
          dDivid = (double)biR3 + (dRem * dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR3 = (int)dQuot;
          /* no break */
        case 3:     // {biR2 - biR0} <- {biR2 - biR0} / divis
          dDivid = (double)biR2 + (dRem * dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR2 = (int)dQuot;
          /* no break */
        case 2:     // {biR1 - biR0} <- {biR1 - biR0} / divis
          dDivid = (double)biR1 + (dRem * dLimbMult);
          dQuot = floor(dDivid / dCurrentPrime);
          dRem = dDivid - (dQuot * dCurrentPrime);
          biR1 = (int)dQuot;
          dDivid = (double)biR0 + (dRem * dLimbMult);
          biR0 = (int)floor(dDivid / dCurrentPrime);
          break;
        default:
          break;
        }
        switch (NumberLengthDividend)
        {
        case 7:
          mostSignificantLimbZero = (biR6 == 0);
          break;
        case 6:
          mostSignificantLimbZero = (biR5 == 0);
          break;
        case 5:
          mostSignificantLimbZero = (biR4 == 0);
          break;
        case 4:
          mostSignificantLimbZero = (biR3 == 0);
          break;
        case 3:
          mostSignificantLimbZero = (biR2 == 0);
          break;
        case 2:    // Criteria is to fit in a double (52 bits).
          mostSignificantLimbZero = (biR1 < (1 << (52 - BITS_PER_GROUP)));
          break;
        default:
          break;
        }
        if (mostSignificantLimbZero)
        {
          NumberLengthDividend--;
          if ((NumberLengthDividend <= 2) && (biR1 < (1 << (52 - BITS_PER_GROUP))))
          {        // Number fits in a double.
            double dDividend = (double)biR1 * (double)(1U << BITS_PER_GROUP) + (double)biR0;
            int sqrtDivid = (int)floor(sqrt(dDividend));
            for (; index < common.siqs.nbrFactorBasePrimes; index++)
            {
              double dDivisor;
              Divisor = rowPrimeSieveData->value;
              dDivisor = (double)Divisor;
              if (testFactorA && (index == newFactorAIndex))
              {
                indexFactorA++;
                if (indexFactorA == common.siqs.nbrFactorsA)
                {
                  testFactorA = false;   // All factors of A were tested.
                }
                else
                {
                  newFactorAIndex = common.siqs.aindex[indexFactorA];
                }
              }
              while (dDividend == floor(dDividend / dDivisor) * dDivisor)
              {           // dDivid is multiple of Divisor.
                dDividend /= dDivisor;
                sqrtDivid = (int)floor(sqrt(dDividend));
                expParity = 1 - expParity;
                if (expParity == 0)
                {
                  rowSquares[nbrSquares] = Divisor;
                  nbrSquares++;
                }
              }
              if (expParity != 0)
              {
                rowMatrixBbeforeMerge[nbrColumns] = index;
                nbrColumns++;
                expParity = 0;
              }
              if (Divisor > sqrtDivid)
              {                     // End of trial division.
                if (dDividend >= (double)(1U << BITS_PER_GROUP))
                {                   // Dividend is too large.
                  return 0;
                }
                Divisor = (int)dDividend;
                rowSquares[0] = nbrSquares;
                index = common.siqs.nbrFactorBasePrimes - 1;
                if ((Divisor <= common.siqs.primeTrialDivisionData[index].value) &&
                  (Divisor > 1))
                {          // Perform binary search to find the index.
                  left = -1;
                  right = common.siqs.nbrFactorBasePrimes;
                  median = right;
                  while (left != right)
                  {
                    median = ((right - left) >> 1) + left;
                    nbr = common.siqs.primeTrialDivisionData[median].value;
                    if (nbr < Divisor)
                    {
                      if ((median == left) &&
                        (congruencesFound >= common.siqs.matrixBLength))
                      {
                        return 0;
                      }
                      left = median;
                    }
                    else if (nbr > Divisor)
                    {
                      right = median;
                    }
                    else
                    {
                      break;
                    }
                  }
                  rowMatrixBbeforeMerge[nbrColumns] = median;
                  nbrColumns++;
                  rowMatrixBbeforeMerge[0] = nbrColumns;
                  return 1;
                }
                rowMatrixBbeforeMerge[0] = nbrColumns;
                return Divisor;
              }
              rowPrimeSieveData++;
            }
            if (dDividend >= (double)(1U << BITS_PER_GROUP))
            {                   // Dividend is too large.
              return 0;
            }
            biR0 = (int)dDividend;
            break;
          }
        }
      }             /* end while */
      rowPrimeSieveData++;
      rowPrimeTrialDivisionData++;
      index++;
    }               /* end while */
  }
  rowSquares[0] = nbrSquares;
  rowMatrixBbeforeMerge[0] = nbrColumns;
  if (NumberLengthDividend > 1)
  {
    return 0;           // Very large quotient.
  }
  return biR0;
}

static void mergeArrays(const int *aindex, int nbrFactorsA, int *rowMatrixB,
  const int *rowMatrixBeforeMerge, int *rowSquares)
{
  int indexAindex = 0;
  int indexRMBBM = 1;
  int indexRMB = 1;
  int nbrColumns = rowMatrixBeforeMerge[0];

  while ((indexAindex < nbrFactorsA) && (indexRMBBM < nbrColumns))
  {
    if (aindex[indexAindex] < rowMatrixBeforeMerge[indexRMBBM])
    {
      rowMatrixB[indexRMB] = aindex[indexAindex];
      indexRMB++;
      indexAindex++;
    }
    else if (aindex[indexAindex] > rowMatrixBeforeMerge[indexRMBBM])
    {
      rowMatrixB[indexRMB] = rowMatrixBeforeMerge[indexRMBBM];
      indexRMB++;
      indexRMBBM++;
    }
    else
    {
      rowSquares[rowSquares[0]] =
        common.siqs.primeTrialDivisionData[aindex[indexAindex]].value;
      indexAindex++;
      rowSquares[0]++;
      indexRMBBM++;
    }
  }
  while (indexAindex < nbrFactorsA)
  {
    rowMatrixB[indexRMB] = aindex[indexAindex];
    indexRMB++;
    indexAindex++;
  }
  while (indexRMBBM < nbrColumns)
  {
    rowMatrixB[indexRMB] = rowMatrixBeforeMerge[indexRMBBM];
    indexRMB++;
    indexRMBBM++;
  }
  rowMatrixB[LENGTH_OFFSET] = indexRMB;
}

static void SmoothRelationFound(
  unsigned char positive,
  int *rowMatrixB, const int *rowMatrixBbeforeMerge,
  int index2,
  int *rowSquares, const limb *biLinearCoeff,
  int nbrLength, limb *biT, limb *biU,
  limb *biR, bool oddPolynomial)
{
  int nbrSquares;
  if (congruencesFound == common.siqs.matrixBLength)
  {
    return;            // All congruences already found.
  }
  // Add all elements of aindex array to the rowMatrixB array discarding
  // duplicates.
  mergeArrays(common.siqs.aindex, common.siqs.nbrFactorsA, rowMatrixB, rowMatrixBbeforeMerge, rowSquares);
  nbrSquares = rowSquares[0];
  IntToBigNbr(1, biR, nbrLength);
  IntToBigNbr(positive ? 1 : -1, biT, nbrLength);
  MultBigNbrByInt(common.siqs.biQuadrCoeff, index2 - common.siqs.SieveLimit, biU,
    nbrLength);                                           // Ax
  AddBigNbrB(biU, biLinearCoeff, biU, nbrLength);         // Ax+B
  if (oddPolynomial)
  {
    SubtractBigNbrB(biU, common.siqs.biLinearDelta[0], biU, nbrLength); // Ax+B (odd)
    SubtractBigNbrB(biU, common.siqs.biLinearDelta[0], biU, nbrLength); // Ax+B (odd)
  }
  if ((uint32_t)biU[nbrLength - 1].x >= (LIMB_RANGE/2U))
  {                                     // If number is negative,
    ChSignBigNbr(biU, nbrLength);       // make it positive.
  }
  for (int index = 1; index < nbrSquares; index++)
  {
    int D = rowSquares[index];
    if (D == common.siqs.multiplier)
    {
      AddBigNbr(biU, common.siqs.Modulus, biU, nbrLength);
      DivBigNbrByInt(biU, D, biU, nbrLength);
    }
    else
    {
      MultBigNbrByInt(biR, D, biR, nbrLength);
    }
  }
  if (InsertNewRelation(rowMatrixB, biT, biU, biR, nbrLength))
  {
    smoothsFound++;
    ShowSIQSStatus();
  }
  return;
}

static void PartialRelationFound(
  unsigned char positive,
  int *rowMatrixB, int *rowMatrixBbeforeMerge,
  int index2,
  int Divid, int *rowPartials,
  int *rowSquares,
  const limb *biLinearCoeff, int nbrLength, limb *biT,
  limb *biR, limb *biU, limb *biV,
  int *indexFactorsA, bool oddPolynomial)
{
  int index;
  int expParity;
  int D;
  int Divisor;
  int nbrFactorsPartial;
  int prev;
  unsigned int seed;
  int hashIndex;
  int *rowPartial;
  int newDivid = Divid;    // This number is greater than zero.
  int indexFactorA = 0;
  int nbrSquares;
  int NumberLengthDivid;
  int biT0;
  int biT1;
  int biT2;
  int biT3;
  int biT4;
  int biT5;
  int biT6;
  int squareRootSize = (nbrLength / 2) + 1;
  int nbrColumns;
  const PrimeTrialDivisionData *rowPrimeTrialDivisionData;

  if (congruencesFound == common.siqs.matrixBLength)
  {
    return;
  }
  // Partial relation found.
  totalPartials++;
  // Check if there is already another relation with the same
  // factor outside the prime base.
  // Calculate hash index
#define SIQS_HASH_MASK ((1U << BITS_SIQS_HASH) - 1U)
  hashIndex = common.siqs.matrixPartialHashIndex[
      ((unsigned int)Divid >> 1) & SIQS_HASH_MASK];
  prev = -1;
  while (hashIndex >= 0)
  {
    int oldDivid;
    
    rowPartial = common.siqs.matrixPartial[hashIndex];
    oldDivid = rowPartial[0];
    if ((newDivid == oldDivid) || (newDivid == -oldDivid))
    {   // Match of partials.
      double dRem;
      double dDivisor;
      for (index = 0; index < squareRootSize; index++)
      {
        biV[index].x = rowPartial[index + 2];
      }                           // biV = Old positive square root (Ax+B).
      for (; index < nbrLength; index++)
      {
        biV[index].x = 0;
      }
      seed = rowPartial[squareRootSize + 2];
      (void)getFactorsOfA(seed, indexFactorsA);
      IntToBigNbr(newDivid, biR, nbrLength);
      nbrFactorsPartial = 0;
      // biT = old (Ax+B)^2.
      MultBigNbr(biV, biV, biT, nbrLength);
      // biT = old (Ax+B)^2 - N.
      SubtractBigNbrB(biT, common.siqs.Modulus, biT, nbrLength);
      if (oldDivid < 0)
      {
        rowPartials[nbrFactorsPartial] = 0; // Insert -1 as a factor.
        nbrFactorsPartial++;
      }
      if ((uint32_t)biT[nbrLength - 1].x >= LIMB_RANGE)
      {
        ChSignBigNbr(biT, nbrLength);   // Make it positive.
      }
      NumberLengthDivid = nbrLength;
      // The number is multiple of the big prime, so divide by it.
      DivBigNbrByInt(biT, newDivid, biT, NumberLengthDivid);
      if (biT[NumberLengthDivid - 1].x == 0)
      {
        NumberLengthDivid--;
      }
      for (index = 0; index < common.siqs.nbrFactorsA; index++)
      {
        DivBigNbrByInt(biT,
          common.siqs.primeTrialDivisionData[*(indexFactorsA+index)].value, biT,
          NumberLengthDivid);
        if (biT[NumberLengthDivid - 1].x == 0)
        {
          NumberLengthDivid--;
        }
      }
      biT0 = biT[0].x;
      biT1 = biT[1].x;
      biT2 = biT[2].x;
      biT3 = biT[3].x;
      biT4 = biT[4].x;
      biT5 = biT[5].x;
      biT6 = biT[6].x;
      for (index = 1; index < common.siqs.nbrFactorBasePrimes; index++)
      {
        expParity = 0;
        if ((index >= common.siqs.indexMinFactorA) &&
            (indexFactorA < common.siqs.nbrFactorsA) && 
            (index == *(indexFactorsA+indexFactorA)))
        {
          expParity = 1;
          indexFactorA++;
        }
        rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[index];
        Divisor = rowPrimeTrialDivisionData->value;
        for (;;)
        {
          switch (NumberLengthDivid)
          {
          case 7:
            dRem = (double)biT6*(double)rowPrimeTrialDivisionData->exp6 +
              (double)biT5*(double)rowPrimeTrialDivisionData->exp5 +
              (double)biT4*(double)rowPrimeTrialDivisionData->exp4 +
              (double)biT3*(double)rowPrimeTrialDivisionData->exp3 +
              (double)biT2*(double)rowPrimeTrialDivisionData->exp2;
            break;
          case 6:
            dRem = (double)biT5*(double)rowPrimeTrialDivisionData->exp5 +
              (double)biT4*(double)rowPrimeTrialDivisionData->exp4 +
              (double)biT3*(double)rowPrimeTrialDivisionData->exp3 +
              (double)biT2*(double)rowPrimeTrialDivisionData->exp2;
            break;
          case 5:
            dRem = (double)biT4*(double)rowPrimeTrialDivisionData->exp4 +
              (double)biT3*(double)rowPrimeTrialDivisionData->exp3 +
              (double)biT2*(double)rowPrimeTrialDivisionData->exp2;
            break;
          case 4:
            dRem = (double)biT3*(double)rowPrimeTrialDivisionData->exp3 +
              (double)biT2*(double)rowPrimeTrialDivisionData->exp2;
            break;
          case 3:
            dRem = (double)biT2*(double)rowPrimeTrialDivisionData->exp2;
            break;
          default:
            dRem = 0.0;
            break;
          }
          dRem += (double)biT1*(double)rowPrimeTrialDivisionData->exp1 + biT0;
          dDivisor = (double)Divisor;
          dRem -= floor(dRem / dDivisor)*dDivisor;
          if (dRem != 0.0)
          {
            break;
          }
          expParity = 1 - expParity;
          DivBigNbrByInt(biT, Divisor, biT, NumberLengthDivid);
          biT0 = biT[0].x;
          biT1 = biT[1].x;
          biT2 = biT[2].x;
          biT3 = biT[3].x;
          biT4 = biT[4].x;
          biT5 = biT[5].x;
          biT6 = biT[6].x;
          if (expParity == 0)
          {
            rowSquares[rowSquares[0]] = Divisor;
            rowSquares[0]++;
          }
          if (NumberLengthDivid <= 2)
          {
            if ((biT0 == 1) && (biT1 == 0))
            {               // biT = 1, so division has ended.
              break;
            }
          }
          else if (biT[NumberLengthDivid - 1].x == 0)
          {
            NumberLengthDivid--;
          }
          else
          {        // Nothing to do.
          }
        }
        if (expParity != 0)
        {
          rowPartials[nbrFactorsPartial] = index;
          nbrFactorsPartial++;
        }
      }
      MultBigNbrByIntB(common.siqs.biQuadrCoeff, index2 - common.siqs.SieveLimit, biT,
        nbrLength);
      AddBigNbrB(biT, biLinearCoeff, biT, nbrLength); // biT = Ax+B
      if (oddPolynomial)
      {                                                     // Ax+B (odd)
        SubtractBigNbrB(biT, common.siqs.biLinearDelta[0], biT, nbrLength);
        SubtractBigNbrB(biT, common.siqs.biLinearDelta[0], biT, nbrLength);
      }
      if ((uint32_t)biT[nbrLength - 1].x >= LIMB_RANGE)
      {                                        // If number is negative
        ChSignBigNbr(biT, nbrLength);   // make it positive.
      }
      // biU = Product of old Ax+B times new Ax+B
      MultBigNbrModN(biV, biT, biU, common.siqs.Modulus, nbrLength);
      // Add all elements of aindex array to the rowMatrixB array discarding
      // duplicates.
      mergeArrays(common.siqs.aindex, common.siqs.nbrFactorsA, rowMatrixB, rowMatrixBbeforeMerge, rowSquares);
      nbrColumns = rowMatrixB[LENGTH_OFFSET];
      rowMatrixBbeforeMerge[0] = nbrColumns;
      {
        int nbrBytes = nbrColumns * (int)sizeof(int);
        (void)memcpy(&rowMatrixBbeforeMerge[1], &rowMatrixB[1], nbrBytes);
      }
      mergeArrays(rowPartials, nbrFactorsPartial, rowMatrixB, rowMatrixBbeforeMerge, rowSquares);
      nbrSquares = rowSquares[0];
      for (index = 1; index < nbrSquares; index++)
      {
        D = rowSquares[index];
        if (D != common.siqs.multiplier)
        {
          MultBigNbrByInt(biR, D, biR, nbrLength);
        }
        else
        {
          AddBigNbr(biU, common.siqs.Modulus, biU, nbrLength);
          DivBigNbrByInt(biU, common.siqs.multiplier, biU, nbrLength);
        }
      }
      if (InsertNewRelation(rowMatrixB, biT, biU, biR, nbrLength))
      {
        partialsFound++;
        ShowSIQSStatus();
      }
      return;
    }
    // No match of partials.
    prev = hashIndex;
    hashIndex = rowPartial[1]; // Get next index for same hash.
  } /* end while */
//  synchronized(firstPrimeSieveData)
  {
    if ((hashIndex == -1) && (nbrPartials < (MAX_PRIMES * 8)))
    { // No match and partials table is not full.
      // Add partial to table of partials.
      if (prev >= 0)
      {
        common.siqs.matrixPartial[prev][1] = nbrPartials;
      }
      else
      {
        common.siqs.matrixPartialHashIndex[
          ((unsigned int)newDivid >> 1) & SIQS_HASH_MASK] = nbrPartials;
      }
      rowPartial = common.siqs.matrixPartial[nbrPartials];
      // Add all elements of aindex array to the rowMatrixB array discarding
      // duplicates.
      mergeArrays(common.siqs.aindex, common.siqs.nbrFactorsA, rowMatrixB, rowMatrixBbeforeMerge, rowSquares);
      IntToBigNbr(Divid, biR, nbrLength);
      nbrSquares = rowSquares[0];
      for (index = 1; index < nbrSquares; index++)
      {
        D = rowSquares[index];
        MultBigNbrByIntModN(biR, D, biR, common.siqs.Modulus, nbrLength);
        if (D == common.siqs.multiplier)
        {
          DivBigNbrByInt(biU, D, biU, nbrLength);
        }
      }
      rowPartial[0] = (positive ? newDivid : -newDivid);
      // Indicate last index with this hash.
      rowPartial[1] = -1;
      MultBigNbrByIntB(common.siqs.biQuadrCoeff, index2 - common.siqs.SieveLimit, biT,
        nbrLength);
      AddBigNbrB(biT, biLinearCoeff, biT, nbrLength); // biT = Ax+B
      if (oddPolynomial)
      {                                                     // Ax+B (odd)
        SubtractBigNbrB(biT, common.siqs.biLinearDelta[0], biT, nbrLength);
        SubtractBigNbrB(biT, common.siqs.biLinearDelta[0], biT, nbrLength);
      }
      if ((uint32_t)biT[nbrLength - 1].x >= LIMB_RANGE)
      {                      // If square root is negative convert to positive.
        ChSignBigNbr(biT, nbrLength);
      }
      for (index = 0; index < squareRootSize; index++)
      {
        rowPartial[index + 2] = biT[index].x;
      }
      rowPartial[squareRootSize + 2] = (int)common.siqs.oldSeed;
      nbrPartials++;
    }
  }               // End synchronized block.
  return;
}

static void SieveLocationHit(int rowMatrixB[], int rowMatrixBbeforeMerge[],
  int index2,
  const PrimeSieveData *primeSieveData,
  int rowPartials[],
  int rowSquares[], limb biDividend[],
  int nbrLength, limb biT[], const limb *biLinearCoeff,
  limb biR[], limb biU[], limb biV[],
  int *indexFactorsA, bool oddPolynomial)
{
  bool positive;
  int NumberLengthDivid;
  int Divid;
  int nbrColumns;
  trialDivisions++;
  MultBigNbrByInt(common.siqs.biQuadrCoeff, index2 - common.siqs.SieveLimit, biT,
    nbrLength);                                      // Ax
  AddBigNbrB(biT, biLinearCoeff, biT, nbrLength);    // Ax+B
  if (oddPolynomial)
  {                                                     // Ax+B (odd)
    SubtractBigNbr(biT, common.siqs.biLinearDelta[0], biT, nbrLength);
    SubtractBigNbr(biT, common.siqs.biLinearDelta[0], biT, nbrLength);
  }
  MultBigNbr(biT, biT, biDividend, nbrLength);       // (Ax+B)^2
                                                        // To factor: (Ax+B)^2-N
  SubtractBigNbrB(biDividend, common.siqs.Modulus, biDividend, nbrLength);
  /* factor biDividend */

  NumberLengthDivid = nbrLength; /* Number length for dividend */
  positive = true;
  if ((uint32_t)biDividend[NumberLengthDivid - 1].x >= LIMB_RANGE)
  { /* Negative */
    positive = false;
    ChSignBigNbr(biDividend, NumberLengthDivid); // Convert to positive
  }
  rowSquares[0] = 1;
  for (int index = 0; index < common.siqs.nbrFactorsA; index++)
  {
    DivBigNbrByInt(biDividend, common.siqs.afact[index], biDividend,
      NumberLengthDivid);
    if (biDividend[NumberLengthDivid - 1].x == 0)
    {
      NumberLengthDivid--;
    }
  }
  nbrColumns = 1;
  if (!positive)
  {                                  // Insert -1 as a factor.
    rowMatrixBbeforeMerge[nbrColumns] = 0;
    nbrColumns++;
  }
  rowMatrixBbeforeMerge[0] = nbrColumns;
  Divid = PerformTrialDivision(primeSieveData,
    rowMatrixBbeforeMerge,
    index2, biDividend,
    rowSquares, NumberLengthDivid,
    oddPolynomial);
  if (Divid == 1)
  { // Smooth relation found.
    SmoothRelationFound(positive, rowMatrixB,
      rowMatrixBbeforeMerge,
      index2,
      rowSquares,
      biLinearCoeff, nbrLength, biT, biU, biR,
      oddPolynomial);
  }
  else
  {
    if ((Divid > 0) && (Divid < common.siqs.largePrimeUpperBound))
    {
      PartialRelationFound(positive, rowMatrixB,
        rowMatrixBbeforeMerge,
        index2,
        Divid, rowPartials,
        rowSquares, biLinearCoeff,
        nbrLength, biT, biR, biU, biV,
        indexFactorsA, oddPolynomial);
    }
  }
  return;
}

static unsigned int getFactorsOfA(unsigned int oldSeed, int *indexA)
{
  unsigned int seed = oldSeed;
  int index;
  int index2;
  int i;
  int tmp;
  for (index = 0; index < common.siqs.nbrFactorsA; index++)
  {
    do
    {
      seed = (1141592621U * seed) + 321435U;
      i = (int)(((double)seed * (double)common.siqs.span)/(double)0x100000000LL + common.siqs.indexMinFactorA);
      for (index2 = 0; index2 < index; index2++)
      {
        if ((indexA[index2] == i) || (indexA[index2] == (i + 1)))
        {
          break;
        }
      }
    } while (index2 < index);
    indexA[index] = i;
  }
  for (index = 0; index<common.siqs.nbrFactorsA; index++)    // Sort factors of A.
  {
    for (index2 = index + 1; index2<common.siqs.nbrFactorsA; index2++)
    {
      if (indexA[index] > indexA[index2])
      {
        tmp = indexA[index];
        indexA[index] = indexA[index2];
        indexA[index2] = tmp;
      }
    }
  }
  return seed;
}

/************************************************************************/
/* Multithread procedure:                                               */
/*                                                                      */
/* 1) Main thread generates factor base and other parameters.           */
/* 2) Start M threads where the number M is specified by the user in a  */
/*    box beneath the applet.                                           */
/* 3) For each polynomial:                                              */
/*    3a) Main thread generates the data for the set of 2^n polynomials.*/
/*    3b) Each child thread computes a range of polynomials             */
/*        (u*2^n/M to (u+1)*2^n/M exclusive).                           */
/* Partial and full relation routines must be synchronized.             */
/************************************************************************/
void FactoringSIQS(const limb *pNbrToFactor, limb *pFactor)
{
  int origNumberLength;
  int FactorBase;
  int currentPrime;
  int NbrMod;
  PrimeSieveData *rowPrimeSieveData;
  PrimeTrialDivisionData *rowPrimeTrialDivisionData;
  int Power2;
  int SqrRootMod;
  int fact;
  int D;
  int E;
  int Q;
  int V;
  int W;
  int X;
  int Y;
  int T1;
  int V1;
  int W1;
  int Y1;
  double Temp;
  double Prod;
  double bestadjust;
  int i;
  int j;
  const int arrmult[] = { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43,
    47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 };
  double adjustment[sizeof(arrmult)/sizeof(arrmult[0])];
  double dNumberToFactor;
  double dlogNumberToFactor;
  double dNbrFactorsA;
  double dNbrFactorBasePrimes;
  double dSieveLimit;
  origNumberLength = NumberLength;
  common.siqs.nbrThreadFinishedPolySet = 0;
  trialDivisions = 0;
  smoothsFound = 0;
  totalPartials = 0;
  partialsFound = 0;
  ValuesSieved = 0;
  congruencesFound = 0;
  polynomialsSieved = 0;
  nbrPartials = 0;
  common.siqs.newSeed = 0;
  common.siqs.nbrPrimesUsed = 0;
  (void)memset(common.siqs.primesUsed, 0, sizeof(common.siqs.primesUsed));
  Temp = logLimbs(pNbrToFactor, origNumberLength);
  dNbrFactorBasePrimes = exp((sqrt(Temp * log(Temp)) * 0.363) - 1.0);
  common.siqs.nbrFactorBasePrimes = (int)dNbrFactorBasePrimes;
  if (common.siqs.nbrFactorBasePrimes > MAX_PRIMES)
  {
    common.siqs.nbrFactorBasePrimes = MAX_PRIMES;
  }
  dSieveLimit = exp(8.5 + (0.015 * Temp));
  common.siqs.SieveLimit = (int)dSieveLimit & 0xFFFFFFF8;
  if (common.siqs.SieveLimit > MAX_SIEVE_LIMIT)
  {
    common.siqs.SieveLimit = MAX_SIEVE_LIMIT;
  }
  dNbrFactorsA = (Temp * 0.051) + 1.0;
  common.siqs.nbrFactorsA = (int)dNbrFactorsA;
  common.siqs.NbrPolynomials = (1 << (common.siqs.nbrFactorsA - 1)) - 1;
  common.siqs.factorSiqs.nbrLimbs = NumberLength;
  common.siqs.factorSiqs.sign = SIGN_POSITIVE;
  {
    int nbrBytes = NumberLength * (int)sizeof(limb);
    (void)memcpy(common.siqs.factorSiqs.limbs, pNbrToFactor, nbrBytes);
  }
  NumberLength = BigNbrToBigInt(&common.siqs.factorSiqs, common.siqs.Modulus);
  common.siqs.Modulus[NumberLength].x = 0;
  NumberLength++;
  common.siqs.Modulus[NumberLength].x = 0;
  {
    int nbrBytes = (NumberLength + 1) * (int)sizeof(int);
    (void)memcpy(common.siqs.TestNbr2, common.siqs.Modulus, nbrBytes);
  }
  (void)memset(common.siqs.matrixPartialHashIndex, 0xFF, sizeof(common.siqs.matrixPartialHashIndex));
#ifdef __EMSCRIPTEN__
  InitSIQSStrings(common.siqs.SieveLimit);
  startSieveTenths = (int)(tenths() - originalTenthSecond);
#endif
  /************************/
  /* Compute startup data */
  /************************/

  // Search for best Knuth-Schroeppel multiplier.
  bestadjust = -10.0;
  common.siqs.primeSieveData[0].value = 1;
  common.siqs.primeTrialDivisionData[0].value = 1;
  rowPrimeSieveData = &common.siqs.primeSieveData[1];
  rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[1];
  rowPrimeSieveData->value = 2;
  rowPrimeTrialDivisionData->value = 2;
  // (2^31)^(j+1) mod 2
  rowPrimeTrialDivisionData->exp1 = 0;
  rowPrimeTrialDivisionData->exp2 = 0;
  rowPrimeTrialDivisionData->exp3 = 0;
  rowPrimeTrialDivisionData->exp4 = 0;
  rowPrimeTrialDivisionData->exp5 = 0;
  rowPrimeTrialDivisionData->exp6 = 0;

  NbrMod = pNbrToFactor->x & 7;
  for (j = 0; j<sizeof(arrmult)/sizeof(arrmult[0]); j++)
  {
    int mod = (NbrMod * arrmult[j]) & 7;
    adjustment[j] = 0.34657359; /*  (ln 2)/2  */
    if (mod == 1)
    {
      adjustment[j] *= 4.0;
    }
    if (mod == 5)
    {
      adjustment[j] *= 2.0;
    }
    adjustment[j] -= log((double)arrmult[j]) / 2.0;
  }
  currentPrime = 3;
  while (currentPrime < 10000)
  {
    NbrMod = RemDivBigNbrByInt(common.siqs.Modulus, currentPrime, NumberLength);
    int jacobi = JacobiSymbol(NbrMod, currentPrime);
    double dp = (double)currentPrime;
    double logp = log(dp) / dp;
    for (j = 0; j<sizeof(arrmult)/sizeof(arrmult[0]); j++)
    {
      if (arrmult[j] == currentPrime)
      {
        adjustment[j] += logp;
      }
      else if (jacobi == JacobiSymbol(arrmult[j], currentPrime))
      {
        adjustment[j] += 2.0 * logp;
      }
      else
      {        // Nothing to do.
      }
    }
    do
    {
      currentPrime += 2;
      for (Q = 3; (Q * Q) <= currentPrime; Q += 2)
      { /* Check if currentPrime is prime */
        if ((currentPrime % Q) == 0)
        {
          break;  /* Composite */
        }
      }
    } while ((Q * Q) <= currentPrime);
  }  /* end while */
  common.siqs.multiplier = 1;
  for (j = 0; j<sizeof(arrmult)/sizeof(arrmult[0]); j++)
  {
    if (adjustment[j] > bestadjust)
    { /* find biggest adjustment */
      bestadjust = adjustment[j];
      common.siqs.multiplier = arrmult[j];
    }
  } /* end while */
  MultBigNbrByInt(common.siqs.TestNbr2, common.siqs.multiplier, common.siqs.Modulus, NumberLength);
  common.siqs.matrixBLength = common.siqs.nbrFactorBasePrimes * 33 / 32;
  if (common.siqs.matrixBLength < (common.siqs.nbrFactorBasePrimes + 50))
  {
    common.siqs.matrixBLength = common.siqs.nbrFactorBasePrimes + 50;
  }
  rowPrimeSieveData->modsqrt = (pNbrToFactor->x & 1) ? 1 : 0;
  switch (common.siqs.Modulus[0].x & 0x07)
  {
  case 1:
    common.siqs.logar2 = (unsigned char)3;
    break;
  case 5:
    common.siqs.logar2 = (unsigned char)1;
    break;
  default:
    common.siqs.logar2 = (unsigned char)1;
    break;
  }
  if ((common.siqs.multiplier != 1) && (common.siqs.multiplier != 2))
  {
    rowPrimeSieveData = &common.siqs.primeSieveData[2];
    rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[2];
    rowPrimeSieveData->value = common.siqs.multiplier;
    rowPrimeTrialDivisionData->value = common.siqs.multiplier;
    rowPrimeSieveData->modsqrt = 0;
    // The following works because multiplier has less than 16 significant bits.
    E = (int)((1U << BITS_PER_GROUP) % common.siqs.multiplier);
    rowPrimeTrialDivisionData->exp1 = E;  // (2^31) mod multiplier
    D = E * E % common.siqs.multiplier;
    rowPrimeTrialDivisionData->exp2 = D;  // (2^31)^2 mod multiplier
    D = D * E % common.siqs.multiplier;
    rowPrimeTrialDivisionData->exp3 = D;  // (2^31)^3 mod multiplier
    D = D * E % common.siqs.multiplier;
    rowPrimeTrialDivisionData->exp4 = D;  // (2^31)^4 mod multiplier
    D = D * E % common.siqs.multiplier;
    rowPrimeTrialDivisionData->exp5 = D;  // (2^31)^5 mod multiplier
    D = D * E % common.siqs.multiplier;
    rowPrimeTrialDivisionData->exp6 = D;  // (2^31)^6 mod multiplier
    j = 3;
  }
  else
  {
    j = 2;
  }
  currentPrime = 3;
  while (j < common.siqs.nbrFactorBasePrimes)
  { /* select small primes */
    NbrMod = RemDivBigNbrByInt(common.siqs.Modulus, currentPrime, NumberLength);
    if ((currentPrime != common.siqs.multiplier) && 
      (JacobiSymbol(NbrMod, currentPrime) == 1))
    {
      double dBase;
      double dPower;
      double dCurrentPrime;
      double dRem;
      /* use only if Jacobi symbol = 0 or 1 */
      rowPrimeSieveData = &common.siqs.primeSieveData[j];
      rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[j];
      rowPrimeSieveData->value = currentPrime;
      rowPrimeTrialDivisionData->value = currentPrime;
      // The following works because multiplier has less than 26 significant bits.
      dBase = (double)((1U << BITS_PER_GROUP) % currentPrime);
      rowPrimeTrialDivisionData->exp1 = (int)dBase;  // (2^31) mod currentPrime
      dCurrentPrime = (double)currentPrime;
      dPower = dBase * dBase;
      dPower -= floor(dPower / dCurrentPrime)*dCurrentPrime;
      rowPrimeTrialDivisionData->exp2 = (int)dPower; // (2^31)^2 mod currentPrime
      dPower *= dBase;
      dPower -= floor(dPower / dCurrentPrime)*dCurrentPrime;
      rowPrimeTrialDivisionData->exp3 = (int)dPower; // (2^31)^3 mod currentPrime
      dPower *= dBase;
      dPower -= floor(dPower / dCurrentPrime)*dCurrentPrime;
      rowPrimeTrialDivisionData->exp4 = (int)dPower; // (2^31)^4 mod currentPrime
      dPower *= dBase;
      dPower -= floor(dPower / dCurrentPrime)*dCurrentPrime;
      rowPrimeTrialDivisionData->exp5 = (int)dPower; // (2^31)^5 mod currentPrime
      dPower *= dBase;
      dPower -= floor(dPower / dCurrentPrime)*dCurrentPrime;
      rowPrimeTrialDivisionData->exp6 = (int)dPower; // (2^31)^6 mod currentPrime
      if ((currentPrime & 3) == 3)
      {
        SqrRootMod = intDoubleModPow(NbrMod, (currentPrime + 1) / 4, currentPrime);
      }
      else if ((currentPrime & 7) == 5)    // currentPrime = 5 (mod 8)
      {
        SqrRootMod =
          intDoubleModPow(NbrMod * 2, (currentPrime - 5) / 8, currentPrime);
        dRem = (double)2 * NbrMod * (double)SqrRootMod;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        dRem = dRem * (double)SqrRootMod - 1;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        dRem = dRem * (double)NbrMod;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        dRem = dRem * (double)SqrRootMod;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        SqrRootMod = (int)dRem;
      }
      else
      {
        Q = currentPrime - 1;
        E = 0;
        Power2 = 1;
        do
        {
          E++;
          Q /= 2;
          Power2 *= 2;
        } while ((Q & 1) == 0); /* E >= 3 */
        Power2 /= 2;
        X = 1;
        do
        {
          X++;
          Y = intDoubleModPow(X, Q, currentPrime);
        } while (intDoubleModPow(Y, Power2, currentPrime) == 1);
        X = intDoubleModPow(NbrMod, (Q - 1) / 2, currentPrime);
        dBase = (double)NbrMod * (double)X;
        V = (int)(dBase - floor(dBase / dCurrentPrime)*dCurrentPrime);
        dBase = (double)V * (double)X;
        W = (int)(dBase - floor(dBase / dCurrentPrime)*dCurrentPrime);
        while (W != 1)
        {
          T1 = 0;
          D = W;
          while (D != 1)
          {
            dBase = (double)D * (double)D;
            D = (int)(dBase - floor(dBase / dCurrentPrime)*dCurrentPrime);
            T1++;
          }
          D = intDoubleModPow(Y, 1 << (E - T1 - 1), currentPrime);
          dBase = (double)D * (double)D;
          Y1 = (int)(dBase - floor(dBase / dCurrentPrime)*dCurrentPrime);
          E = T1;
          dBase = (double)V * (double)D;
          V1 = (int)(dBase - floor(dBase / dCurrentPrime)*dCurrentPrime);
          dBase = (double)W * (double)Y1;
          W1 = (int)(dBase - floor(dBase / dCurrentPrime)*dCurrentPrime);
          Y = Y1;
          V = V1;
          W = W1;
        } /* end while */
        SqrRootMod = V;
      } /* end if */
      rowPrimeSieveData->modsqrt = SqrRootMod;
      j++;
    } /* end while */
    do
    {
      currentPrime += 2;
      for (Q = 3; (Q * Q) <= currentPrime; Q += 2)
      { /* Check if currentPrime is prime */
        if ((currentPrime % Q) == 0)
        {
          break;  /* Composite */
        }
      }
    } while ((Q * Q) <= currentPrime);
  } /* End while */

  FactorBase = currentPrime;
  common.siqs.largePrimeUpperBound = 100 * FactorBase;
  // Convert array of limbs to BigInteger and find its logarithm.
  dlogNumberToFactor = logLimbs(pNbrToFactor, origNumberLength);
  dNumberToFactor = exp(dlogNumberToFactor);
#ifdef __EMSCRIPTEN__
  getMultAndFactorBase(common.siqs.multiplier, FactorBase);
  databack(lowerText);
#endif
  common.siqs.firstLimit = 2;
  for (j = 2; j < common.siqs.nbrFactorBasePrimes; j++)
  {
    common.siqs.firstLimit *= (common.siqs.primeSieveData[j].value);
    if (common.siqs.firstLimit > (2 * common.siqs.SieveLimit))
    {
      break;
    }
  }
  dNumberToFactor *= common.siqs.multiplier;
  common.siqs.smallPrimeUpperLimit = j + 1;
  common.siqs.threshold =
    (unsigned char)((log(
      sqrt(dNumberToFactor) * common.siqs.SieveLimit /
      (FactorBase * 64) /
      common.siqs.primeSieveData[j + 1].value)
      / LOG_3) + 0x80);
  common.siqs.firstLimit = (int)(log(dNumberToFactor) / 3);
  for (common.siqs.secondLimit = common.siqs.firstLimit;
      common.siqs.secondLimit < common.siqs.nbrFactorBasePrimes;
      common.siqs.secondLimit++)
  {
    if ((common.siqs.primeSieveData[common.siqs.secondLimit].value * 2) >
        common.siqs.SieveLimit)
    {
      break;
    }
  }
  for (common.siqs.thirdLimit = common.siqs.secondLimit;
      common.siqs.thirdLimit < common.siqs.nbrFactorBasePrimes;
      common.siqs.thirdLimit++)
  {
    if (common.siqs.primeSieveData[common.siqs.thirdLimit].value >
        (2 * common.siqs.SieveLimit))
    {
      break;
    }
  }
  common.siqs.nbrPrimes2 = common.siqs.nbrFactorBasePrimes - 4;
  Prod = sqrt(2 * dNumberToFactor) / (double)common.siqs.SieveLimit;
  fact = (int)pow(Prod, 1.0 / (double)common.siqs.nbrFactorsA);
  i = 2;
  while (common.siqs.primeSieveData[i].value <= fact)
  {
    i++;
  }
  common.siqs.span = common.siqs.nbrFactorBasePrimes / (2 * common.siqs.nbrFactorsA*common.siqs.nbrFactorsA);
  if (common.siqs.nbrFactorBasePrimes < 500)
  {
    common.siqs.span *= 2;
  }
  common.siqs.indexMinFactorA = i - (common.siqs.span / 2);
  /*********************************************/
  /* Generate sieve threads                    */
  /*********************************************/
  sieveThread(&common.siqs.TempResult);
  NumberLength = origNumberLength;
  {
    int nbrBytes = common.siqs.TempResult.nbrLimbs * (int)sizeof(limb);
    (void)memcpy(pFactor, common.siqs.TempResult.limbs, nbrBytes);
  }
  {
    int nbrBytes = (NumberLength - common.siqs.TempResult.nbrLimbs) * (int)sizeof(limb);
    (void)memset(pFactor + common.siqs.TempResult.nbrLimbs, 0, nbrBytes);
  }

#if 0
  for (threadNumber = 0; threadNumber<numberThreads; threadNumber++)
  {
    //new Thread(this).start();                // Start new thread.
    //synchronized(amodq)
    {
//      while ((threadArray[threadNumber] == null) &&
//        (getTerminateThread() == FALSE))
      {
        //try
        //{
        //  amodq.wait();
        //}
        //catch (InterruptedException ie) {}
      }
    }
  }
  //synchronized(matrixB)
  {
    while ((common.siqs.factorSiqs == null) && (getTerminateThread() == FALSE))
    {
      try
      {
        matrixB.wait();
      }
      catch (InterruptedException ie) {}
    }
  }
  if (/*getTerminateThread() ||*/ ((common.siqs.TempResult.nbrLimbs == 1) && (common.siqs.TempResult.limbs[0].x == 0)))
  {
    //throw new ArithmeticException();
  }
#endif
#if 0
  for (threadNumber = 0; threadNumber<numberThreads; threadNumber++)
  {                 // Wake up all sieve threads so they can terminate.
    if (threadArray[threadNumber].isAlive())
    {
      //try
      {
        threadArray[threadNumber].interrupt();
      }
      //catch (Exception e) {}
    }
  }
#endif
#if 0
  synchronized(this)
  {
    saveSIQSStatistics(polynomialsSieved, trialDivisions,
      smoothsFound, totalPartials,
      partialsFound, ValuesSieved);
  }
  return common.siqs.factorSiqs;
#endif
}

void ShowSIQSStatus(void)
{
#ifdef __EMSCRIPTEN__
  int elapsedTime = (int)(tenths() - originalTenthSecond);
  if ((elapsedTime / 10) != (oldTimeElapsed / 10))
  {
    oldTimeElapsed = elapsedTime;
    ShowSIQSInfo((elapsedTime - startSieveTenths)/10, congruencesFound,
      common.siqs.matrixBLength, elapsedTime / 10);
  }
#endif
}

static bool InsertNewRelation(
  const int* rowMatrixB,
  limb* biT, limb* squareRightHandSide, limb* squareLeftHandSide,
  int NumberLengthMod)
{
  int k;
  int lenDivisor;
  int nbrColumns = rowMatrixB[LENGTH_OFFSET];
  // Insert it only if it is different from previous relations.
  if (rowMatrixB[0] <= 1)
  {
    return false;
  }
  if (congruencesFound >= common.siqs.matrixBLength)
  {                   // Discard excess congruences.
    return false;
  }
#if DEBUG_SIQS == 1
  char* ptrOutput = output;
#ifdef __EMSCRIPTEN__
  *ptrOutput = '9';
  ptrOutput++;
#endif
  copyStr(&ptrOutput, "Mod(");
  for (int i = 1; i < *rowMatrixB; i++)
  {
    if (i != 1)
    {
      *ptrOutput = '*';
      ptrOutput++;
    }
    if (*(rowMatrixB + i) == 0)
    {
      copyStr(&ptrOutput, "(-1)");
    }
    else
    {
      int2dec(&ptrOutput, common.siqs.primeSieveData[*(rowMatrixB + i)].value);
    }
  }
  *ptrOutput = ' ';
  ptrOutput++;
  *ptrOutput = '-';
  ptrOutput++;
  *ptrOutput = ' ';
  ptrOutput++;
#endif
  // Check whether this relation is already in the matrix.
  const int* curRowMatrixB = common.siqs.matrixB[0];
  for (int i = 0; i < congruencesFound; i++)
  {
    if (nbrColumns == *(curRowMatrixB + LENGTH_OFFSET))
    {
      for (k = 1; k < nbrColumns; k++)
      {
        if (*(rowMatrixB + k) != curRowMatrixB[k])
        {
          break;
        }
      }
      if (k == nbrColumns)
      {
        return false; // Do not insert same relation.
      }
    }
    curRowMatrixB += MAX_FACTORS_RELATION;
  }
  /* Convert negative numbers to the range 0 <= n < Modulus */
  if ((common.siqs.Modulus[0].x & 1) == 0)
  {             // Even modulus.
    DivBigNbrByInt(common.siqs.Modulus, 2, common.siqs.TestNbr2, NumberLengthMod);
    // If biR >= Modulus perform biR = biR - Modulus.
    for (k = 0; k < NumberLengthMod; k++)
    {
      biT[k].x = 0;
    }
    lenDivisor = NumberLengthMod;
    if (common.siqs.Modulus[lenDivisor - 1].x == 0)
    {
      lenDivisor--;
    }
    AddBigIntModN(squareLeftHandSide, biT, squareLeftHandSide,
      common.siqs.TestNbr2, lenDivisor);
    ModInvBigInt(squareLeftHandSide, biT, common.siqs.TestNbr2, lenDivisor);
  }
  else
  {             // Odd modulus
    lenDivisor = NumberLengthMod;
    if (common.siqs.Modulus[lenDivisor - 1].x == 0)
    {
      lenDivisor--;
    }
    ModInvBigInt(squareLeftHandSide, biT, common.siqs.Modulus, lenDivisor);
  }
  if ((squareRightHandSide[NumberLengthMod - 1].x & HALF_INT_RANGE) != 0)
  {             // If negative, convert to positive.
    AddBigNbr(squareRightHandSide, common.siqs.Modulus, squareRightHandSide, NumberLengthMod);
  }
  squareRightHandSide[NumberLengthMod].x = 0;
  AdjustModN(squareRightHandSide, common.siqs.Modulus, lenDivisor);
  // Compute biU / biR  (mod Modulus)
  MultBigNbrModN(squareRightHandSide, biT, squareLeftHandSide, common.siqs.Modulus, lenDivisor);

  // Add relation to matrix B.
  {
    int nbrBytes = nbrColumns * (int)sizeof(int);
    (void)memcpy(common.siqs.matrixB[congruencesFound], &rowMatrixB[0], nbrBytes);
  }
  {
    int nbrBytes = NumberLengthMod * (int)sizeof(int);
    (void)memcpy(common.siqs.vectLeftHandSide[congruencesFound], squareLeftHandSide, nbrBytes);
  }
  congruencesFound++;
  nbrColumns = *(rowMatrixB + LENGTH_OFFSET);
  for (k = 1; k < nbrColumns; k++)
  {
    if (common.siqs.primesUsed[*(rowMatrixB + k)] == 0)
    {
      common.siqs.primesUsed[*(rowMatrixB + k)] = 1;
      common.siqs.nbrPrimesUsed++;
    }
  }
#if DEBUG_SIQS == 1
  static BigInteger k1;
  (void)memcpy(k1.limbs, squareLeftHandSide, NumberLength * sizeof(limb));
  k1.nbrLimbs = NumberLength;
  k1.sign = SIGN_POSITIVE;
  BigInteger2Dec(&ptrOutput, &k1, 0);
  ShowSquareModP(ptrOutput);
#endif
  return true;
}

static int intModInv(int NbrMod, int currentPrime)
{
  int QQ;
  int T1;
  int T3;
  int V1 = 1;
  int V3 = NbrMod;
  int U1 = 0;
  int U3 = currentPrime;
  while (V3 != 0)
  {
    int subt = U3 - V3 - V3;
    if (subt < 0)
    {               // QQ = 1 (probability = 41.5 %)
      T1 = U1 - V1;
      T3 = U3 - V3;
    }
    else if (subt < V3)
    {               // QQ = 2 (probability = 17 %)
      T1 = U1 - V1 - V1;
      T3 = subt;
    }
    else
    {               // QQ > 2 (probability = 41.5 %)
      QQ = U3 / V3;
      T1 = U1 - (V1 * QQ);
      T3 = U3 - (V3 * QQ);
    }
    U1 = V1;
    U3 = V3;
    V1 = T1;
    V3 = T3;
  }
  return U1 + (currentPrime & (U1 >> 31));
}

/****************/
/* Sieve thread */
/****************/
static void sieveThread(BigInteger *result)
{
  limb biT[MAX_LIMBS_SIQS];
  limb biU[MAX_LIMBS_SIQS];
  limb biV[MAX_LIMBS_SIQS];
  limb biR[MAX_LIMBS_SIQS];
  PrimeSieveData *rowPrimeSieveData;
  const PrimeSieveData *rowPrimeSieveData0;
  const PrimeTrialDivisionData *rowPrimeTrialDivisionData;
  short SieveArray[2*MAX_SIEVE_LIMIT];
  int rowPartials[200];
  limb biLinearCoeff[MAX_LIMBS_SIQS];
  limb biDividend[MAX_LIMBS_SIQS];
  limb biAbsLinearCoeff[MAX_LIMBS_SIQS];
  int indexFactorsA[50];
  int rowSquares[200];
  int polynomialsPerThread = ((common.siqs.NbrPolynomials - 1) / numberThreads) & 0xFFFFFFFE;
  int firstPolynomial = common.siqs.threadNumber*polynomialsPerThread;
  int lastPolynomial = firstPolynomial + polynomialsPerThread;
  firstPolynomial |= 1;
  int grayCode = firstPolynomial ^ (firstPolynomial >> 1);
  firstPolynomial++;
  int i;
  int PolynomialIndex;
  int index;
  int index2;
  int currentPrime;
  int RemB;
  int D;
  int Q;
  const limb *piDividend;
  int rowMatrixBbeforeMerge[200];
  int rowMatrixB[200];
  bool positive;
  int inverseA;
  int twiceInverseA;
  int NumberLengthA;
  int NumberLengthB;

  (void)memset(indexFactorsA, 0, sizeof(indexFactorsA));
  (void)memset(biLinearCoeff, 0, sizeof(biLinearCoeff));
//  synchronized(amodq)
  {
    if (common.siqs.threadNumber != 0)
    {
#if 0
      for (i = 0; i<common.siqs.nbrFactorBasePrimes; i++)
      {
        rowPrimeSieveData = &common.siqs.primeSieveData[i];
        rowPrimeSieveData0 = &common.siqs.primeSieveData[i];
        rowPrimeSieveData->value = rowPrimeSieveData0->value;
        rowPrimeSieveData->modsqrt = rowPrimeSieveData0->modsqrt;
        (void)memcpy(rowPrimeSieveData->Bainv2, rowPrimeSieveData0->Bainv2, sizeof(rowPrimeSieveData0->Bainv2));
      }
#endif
    }
  }               // End synchronized block.
    for (int polySet = 1;; polySet++)
    {                         // For each polynomial set...
#if 0
      //if (getTerminateThread())
      //{
      //  throw new ArithmeticException();
      //}
      //synchronized(amodq)
#endif
      {
        common.siqs.nbrThreadFinishedPolySet++;
        if (congruencesFound >= common.siqs.matrixBLength/* || common.siqs.factorSiqs != null*/)
        {
          if (common.siqs.nbrThreadFinishedPolySet < (polySet * numberThreads))
          {
            return;
          }
          if (1/*common.siqs.factorSiqs == null*/)
          {
            while (!LinearAlgebraPhase(biT, biR, biU, NumberLength))
            {    // Nothing to do.
            }
            BigIntToBigNbr(result, biT, NumberLength);  // Factor found.
#if 0
            synchronized(matrixB)
            {
              common.siqs.factorSiqs = result;
              matrixB.notify();
            }
#endif
          }
          else
          {
#if 0
            synchronized(matrixB)
            {
              matrixB.notify();
            }
#endif
          }
          return;
        }
        if (common.siqs.nbrThreadFinishedPolySet == (polySet * numberThreads))
        {
          /*********************************************/
          /* Initialization stage for first polynomial */
          /*********************************************/
          common.siqs.firstPrimeSieveData = common.siqs.primeSieveData;
          common.siqs.oldSeed = common.siqs.newSeed;
          common.siqs.newSeed = getFactorsOfA(common.siqs.oldSeed, common.siqs.aindex);
#if DEBUG_SIQS == 5
          char out[1000];
          char* ptrOutput = out;
          copyStr(&ptrOutput, "A = ");
#endif
          for (index = 0; index<common.siqs.nbrFactorsA; index++)
          {                        // Get the values of the factors of A.
            common.siqs.afact[index] = common.siqs.primeSieveData[common.siqs.aindex[index]].value;
#if DEBUG_SIQS == 5
            int2dec(&ptrOutput, common.siqs.afact[index]);
            copyStr(&ptrOutput, " * ");
#endif
          }
#if DEBUG_SIQS == 5
          ptrOutput -= 3;
          *ptrOutput = 0;
#ifdef __EMSCRIPTEN__
          databack(output);
#else
          printf("%s\n", out);
#endif
#endif
          // Compute the leading coefficient in biQuadrCoeff.

          IntToBigNbr(common.siqs.afact[0], common.siqs.biQuadrCoeff, NumberLength);
          for (index = 1; index < common.siqs.nbrFactorsA; index++)
          {
            MultBigNbrByInt(common.siqs.biQuadrCoeff, common.siqs.afact[index], common.siqs.biQuadrCoeff,
              NumberLength);
          }
          for (NumberLengthA = NumberLength; NumberLengthA >= 2; NumberLengthA--)
          {
            if (common.siqs.biQuadrCoeff[NumberLengthA - 1].x != 0)
            {
              break;
            }
          }
          for (index = 0; index < common.siqs.nbrFactorsA; index++)
          {
            currentPrime = common.siqs.afact[index];
            D = RemDivBigNbrByInt(common.siqs.biQuadrCoeff,
              currentPrime*currentPrime, NumberLengthA) / currentPrime;
            Q = common.siqs.primeSieveData[common.siqs.aindex[index]].modsqrt *
              intModInv(D, currentPrime) % currentPrime;
            common.siqs.amodq[index] = D << 1;
            common.siqs.tmodqq[index] = RemDivBigNbrByInt(common.siqs.Modulus,
              currentPrime*currentPrime, NumberLength);
            if ((Q + Q) > currentPrime)
            {
              Q = currentPrime - Q;
            }
            DivBigNbrByInt(common.siqs.biQuadrCoeff, currentPrime, biDividend,
              NumberLengthA);
            MultBigNbrByInt(biDividend, Q, common.siqs.biLinearDelta[index],
              NumberLengthA);
            for (index2 = NumberLengthA; index2 < NumberLength; index2++)
            {
              common.siqs.biLinearDelta[index][index2].x = 0;
            }
          }
          for (index = 1; index < common.siqs.nbrFactorBasePrimes; index++)
          {
            double dRem;
            double dCurrentPrime;
            rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[index];
            rowPrimeSieveData = &common.siqs.primeSieveData[index];
            // Get current prime.
            currentPrime = rowPrimeTrialDivisionData->value;
            piDividend = common.siqs.biQuadrCoeff;   // Get A mod current prime.
            switch (NumberLengthA)
            {
            case 7:
              dRem = (double)(piDividend + 6)->x * (double)rowPrimeTrialDivisionData->exp6 +
                (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
                (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
              break;
            case 6:
              dRem = (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
                (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;              break;
            case 5:
              dRem = (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
              break;
            case 4:
              dRem = (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
              break;
            default:
              dRem = (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
              break;
            }
            dRem += (double)piDividend->x;
            dCurrentPrime = (double)currentPrime;
            dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
            // Get its inverse
            inverseA = intModInv((int)dRem, currentPrime);
            twiceInverseA = inverseA << 1;       // and twice this value.
            dRem = (double)twiceInverseA * (double)rowPrimeSieveData->modsqrt;
            dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
            rowPrimeSieveData->difsoln = (int)dRem;
            switch (NumberLengthA)
            {
            case 7:
              for (index2 = common.siqs.nbrFactorsA - 1; index2 > 0; index2--)
              {
                piDividend = common.siqs.biLinearDelta[index2];
                dRem = (double)(piDividend + 6)->x * (double)rowPrimeTrialDivisionData->exp6 +
                  (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
                  (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                  (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                  (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                  (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                  (double)piDividend->x;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                dRem *= twiceInverseA;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                common.siqs.Bainv2[index2 - 1][index] = (int)dRem;
              }
              piDividend = common.siqs.biLinearDelta[0];
              dRem = (double)(piDividend + 6)->x * (double)rowPrimeTrialDivisionData->exp6 +
                (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
                (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                (double)piDividend->x;
              dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
              break;
            case 6:
              for (index2 = common.siqs.nbrFactorsA - 1; index2 > 0; index2--)
              {
                piDividend = common.siqs.biLinearDelta[index2];
                dRem = (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
                  (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                  (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                  (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                  (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                  (double)piDividend->x;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                dRem *= twiceInverseA;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                common.siqs.Bainv2[index2 - 1][index] = (int)dRem;
              }
              piDividend = common.siqs.biLinearDelta[0];
              dRem = (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
                (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                (double)piDividend->x;
              dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
              break;
            case 5:
              for (index2 = common.siqs.nbrFactorsA - 1; index2 > 0; index2--)
              {
                piDividend = common.siqs.biLinearDelta[index2];
                dRem = (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                  (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                  (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                  (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                  (double)piDividend->x;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                dRem *= twiceInverseA;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                common.siqs.Bainv2[index2 - 1][index] = (int)dRem;
              }
              piDividend = common.siqs.biLinearDelta[0];
              dRem = (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
                (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                (double)piDividend->x;
              dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
              break;
            case 4:
              for (index2 = common.siqs.nbrFactorsA - 1; index2 > 0; index2--)
              {
                piDividend = common.siqs.biLinearDelta[index2];
                dRem = (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                  (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                  (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                  (double)piDividend->x;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                dRem *= twiceInverseA;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                common.siqs.Bainv2[index2 - 1][index] = (int)dRem;
              }
              piDividend = common.siqs.biLinearDelta[0];
              dRem = (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
                (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                (double)piDividend->x;
              dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
              break;
            default:
              for (index2 = common.siqs.nbrFactorsA - 1; index2 > 0; index2--)
              {
                piDividend = common.siqs.biLinearDelta[index2];
                dRem = (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                  (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                  (double)piDividend->x;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                dRem *= twiceInverseA;
                dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
                common.siqs.Bainv2[index2 - 1][index] = (int)dRem;
              }
              piDividend = common.siqs.biLinearDelta[0];
              dRem = (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
                (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1 +
                (double)piDividend->x;
              dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
              break;
            }
            dRem *= twiceInverseA;
            dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
            rowPrimeSieveData->Bainv2_0 = (int)dRem;
            if (rowPrimeSieveData->Bainv2_0 != 0)
            {
              rowPrimeSieveData->Bainv2_0 =
                currentPrime - rowPrimeSieveData->Bainv2_0;
            }
          }
          for (index2 = 0; index2 < common.siqs.nbrFactorsA; index2++)
          {
            common.siqs.primeSieveData[common.siqs.aindex[index2]].difsoln = -1; // Do not sieve.
          }
#if 0
          //synchronized(TestNbr2)
          {
            //TestNbr2.notifyAll();
          }
#endif
        }           // End initializing first polynomial
      }             // End synchronized
#if 0
      synchronized(TestNbr2)
      {
        while (nbrThreadFinishedPolySet < polySet * numberThreads)
        {
          try
          {
            TestNbr2.wait();
          }
          catch (InterruptedException ie) {}
        }
      }
#endif
      if (congruencesFound >= common.siqs.matrixBLength)
      {
        if (common.siqs.nbrThreadFinishedPolySet > (numberThreads*polySet))
        {
          continue;
        }
        //synchronized(amodq)
        {
          common.siqs.nbrThreadFinishedPolySet++;
        }
        return;
      }
      PolynomialIndex = firstPolynomial;
      // Compute first polynomial parameters.
      for (i = 0; i<NumberLength; i++)
      {
        biLinearCoeff[i].x = common.siqs.biLinearDelta[0][i].x;
      }
      for (i = 1; i<common.siqs.nbrFactorsA; i++)
      {
        if ((grayCode & (1 << i)) == 0)
        {
          AddBigNbrB(biLinearCoeff, common.siqs.biLinearDelta[i], biLinearCoeff,
            NumberLength);
        }
        else
        {
          SubtractBigNbrB(biLinearCoeff, common.siqs.biLinearDelta[i], biLinearCoeff,
            NumberLength);
        }
      }
      for (NumberLengthA = NumberLength; NumberLengthA >= 2; NumberLengthA--)
      {
        if (common.siqs.biQuadrCoeff[NumberLengthA - 1].x != 0)
        {                             // Go out if significant limb.
          break;
        }
      }
      if ((uint32_t)biLinearCoeff[NumberLength - 1].x >= LIMB_RANGE)
      {                               // Number is negative.
        positive = false;
        {
          int nbrBytes = NumberLength * (int)sizeof(biT[0]);
          (void)memcpy(biT, biLinearCoeff, nbrBytes);
        }        
        ChSignBigNbr(biT, NumberLength);   // Make it positive.
        (void)memcpy(biAbsLinearCoeff, biT, sizeof(biT));
      }
      else
      {
        positive = true;                       // B is positive.
                                               // Get B mod current prime. 
        (void)memcpy(biAbsLinearCoeff, biLinearCoeff, sizeof(biLinearCoeff));
      }
      for (NumberLengthB = NumberLength; NumberLengthB >= 2; NumberLengthB--)
      {
        if (biAbsLinearCoeff[NumberLengthB - 1].x != 0)
        {                                // Go out if significant limb.
          break;
        }
      }
      for (i = common.siqs.nbrFactorBasePrimes - 1; i>0; i--)
      {
        double dRem;
        double dCurrentPrime;
        rowPrimeSieveData = &common.siqs.primeSieveData[i];
        rowPrimeSieveData0 = common.siqs.firstPrimeSieveData+i;
        rowPrimeSieveData->difsoln = rowPrimeSieveData0->difsoln;
        rowPrimeSieveData->Bainv2_0 = rowPrimeSieveData0->Bainv2_0;
        rowPrimeTrialDivisionData = &common.siqs.primeTrialDivisionData[i];
        currentPrime = rowPrimeTrialDivisionData->value;     // Get current prime.
         // Get A mod current prime.
        piDividend = common.siqs.biQuadrCoeff;
        switch (NumberLengthA)
        {
        case 7:
          dRem = (double)(piDividend + 6)->x * (double)rowPrimeTrialDivisionData->exp6 +
            (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
            (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
            (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 6:
          dRem = (double)(piDividend + 5)->x * (double)rowPrimeTrialDivisionData->exp5 +
            (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
            (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 5:
          dRem = (double)(piDividend + 4)->x * (double)rowPrimeTrialDivisionData->exp4 +
            (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 4:
          dRem = (double)(piDividend + 3)->x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 3:
          dRem = (double)(piDividend + 2)->x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        default:
          dRem = (double)(piDividend + 1)->x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        }
        dCurrentPrime = (double)currentPrime;
        dRem += (double)piDividend->x;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        // Get its inverse
        inverseA = intModInv((int)dRem, currentPrime);
        switch (NumberLengthB)
        {
        case 7:
          dRem = (double)biAbsLinearCoeff[6].x * (double)rowPrimeTrialDivisionData->exp6 +
            (double)biAbsLinearCoeff[5].x * (double)rowPrimeTrialDivisionData->exp5 +
            (double)biAbsLinearCoeff[4].x * (double)rowPrimeTrialDivisionData->exp4 +
            (double)biAbsLinearCoeff[3].x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biAbsLinearCoeff[2].x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biAbsLinearCoeff[1].x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 6:
          dRem = (double)biAbsLinearCoeff[5].x * (double)rowPrimeTrialDivisionData->exp5 +
            (double)biAbsLinearCoeff[4].x * (double)rowPrimeTrialDivisionData->exp4 +
            (double)biAbsLinearCoeff[3].x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biAbsLinearCoeff[2].x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biAbsLinearCoeff[1].x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 5:
          dRem = (double)biAbsLinearCoeff[4].x * (double)rowPrimeTrialDivisionData->exp4 +
            (double)biAbsLinearCoeff[3].x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biAbsLinearCoeff[2].x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biAbsLinearCoeff[1].x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 4:
          dRem = (double)biAbsLinearCoeff[3].x * (double)rowPrimeTrialDivisionData->exp3 +
            (double)biAbsLinearCoeff[2].x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biAbsLinearCoeff[1].x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        case 3:
          dRem = (double)biAbsLinearCoeff[2].x * (double)rowPrimeTrialDivisionData->exp2 +
            (double)biAbsLinearCoeff[1].x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        default:
          dRem = (double)biAbsLinearCoeff[1].x * (double)rowPrimeTrialDivisionData->exp1;
          break;
        }
        dRem += (double)biAbsLinearCoeff[0].x;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        RemB = (int)dRem;
        if (positive)
        {
          RemB = currentPrime - RemB;
        }
        dRem = (double)inverseA * (double)(rowPrimeSieveData0->modsqrt + RemB) +
               (double)common.siqs.SieveLimit;
        dRem -= floor(dRem / dCurrentPrime)*dCurrentPrime;
        rowPrimeSieveData->soln1 = (int)dRem;
      }
      do
      {                       // For each polynomial...
        if (congruencesFound >= common.siqs.matrixBLength /*|| common.siqs.factorSiqs != null*/)
        {
          if (common.siqs.nbrThreadFinishedPolySet > (numberThreads*polySet))
          {
            break;
          }
          //synchronized(amodq)
          {
            common.siqs.nbrThreadFinishedPolySet++;
          }
          return;             // Another thread finished factoring.
        }
        polynomialsSieved += 2;
        /***************/
        /* Sieve stage */
        /***************/
        PerformSiqsSieveStage(common.siqs.primeSieveData, SieveArray,
          PolynomialIndex,
          biLinearCoeff,
          NumberLength);
        ValuesSieved += 2 * common.siqs.SieveLimit;
        /************************/
        /* Trial division stage */
        /************************/
        SieveArray[0] = 0;    // Overwrite incorrect value.
        index2 = (2 * common.siqs.SieveLimit) - 1;
        do
        {
          index2 -= 16;
          const int32_t *ptr = (int32_t *)&SieveArray[index2 + 1];
          if (((*ptr | *(ptr + 1) | *(ptr + 2) | *(ptr+3) |
            *(ptr + 4) | *(ptr + 5) | *(ptr + 6) | *(ptr + 7)) & 0x80808080) != 0)
          {
            for (i = 16; i>0; i--)
            {
              if ((SieveArray[index2 + i] & 0x80) != 0)
              {
                if (congruencesFound >= common.siqs.matrixBLength)
                {       // All congruences were found: stop sieving.
                  index2 = 0;
                  break;
                }
                SieveLocationHit(rowMatrixB,
                  rowMatrixBbeforeMerge,
                  index2 + i, common.siqs.primeSieveData,
                  rowPartials,
                  rowSquares,
                  biDividend, NumberLength, biT,
                  biLinearCoeff, biR, biU, biV,
                  indexFactorsA, false);
                if (congruencesFound >= common.siqs.matrixBLength)
                {               // All congruences were found: stop sieving.
                  index2 = 0;
                  break;
                }
              }
              if (SieveArray[index2 + i] < 0)
              {
                if (congruencesFound >= common.siqs.matrixBLength)
                {       // All congruences were found: stop sieving.
                  index2 = 0;
                  break;
                }
                SieveLocationHit(rowMatrixB,
                  rowMatrixBbeforeMerge,
                  index2 + i, common.siqs.primeSieveData,
                  rowPartials,
                  rowSquares,
                  biDividend, NumberLength, biT,
                  biLinearCoeff, biR, biU, biV,
                  indexFactorsA, true);
                if (congruencesFound >= common.siqs.matrixBLength)
                {               // All congruences were found: stop sieving.
                  index2 = 0;
                  break;
                }
              }
            }
          }
        } while (index2 > 0);
        /*******************/
        /* Next polynomial */
        /*******************/
        PolynomialIndex += 2;
      } while ((PolynomialIndex <= lastPolynomial) &&
        (congruencesFound < common.siqs.matrixBLength));
    }
#if 0
  }
  catch (ArithmeticException ae)
  {
    synchronized(matrixB)
    {
      common.siqs.factorSiqs = null;
      matrixB.notify();
    }
  }
#endif
}
#if 0
/* Implementation of algorithm explained in Gower and Wagstaff paper */
/* The variables with suffix 3 correspond to multiplier = 3 */
static int SQUFOF(double N, int queue[])
{
  double squareRoot;
  int Q, Q1, P, P1, L, S;
  int ctr, j, r, s, t, q;
  int queueHead, queueTail, queueIndex;
  double N3;
  int Q3, Q13, P3, P13, L3, S3;
  int r3, s3, t3, q3;
  int queueHead3, queueTail3, queueIndex3;
  int QRev, Q1Rev, PRev, P1Rev;
  int tRev, qRev, uRev;
  /* Step 1: Initialize */
  squareRoot = sqrt(N);
  S = (int)squareRoot;
  if ((double)(S + 1)*(double)(S + 1) <= N)
  {
    S++;
  }
  if ((double)S*(double)S > N)
  {
    S--;
  }
  if ((double)S*(double)S == N)
  {
    return S;
  }
  N3 = 3 * N;
#ifdef _USING64BITS_
  if ((N & 3) == 1)
  {
    N <<= 1;
  }
  if ((N3 & 3) == 1)
  {
    N3 <<= 1;
  }
#else
  if (N - floor(N / 4) == 1)
  {
    N *= 2;
  }
  if (N3 - floor(N3 / 4) == 1)
  {
    N3 *= 2;
  }
#endif
  squareRoot = sqrt(N);
  S = (int)squareRoot;
  if ((double)(S + 1)*(double)(S + 1) <= N)
  {
    S++;
  }
  if ((double)S*(double)S > N)
  {
    S--;
  }
  Q1 = 1;
  P = S;
  Q = (int)N - P*P;
  L = (int)(2 * sqrt(2 * squareRoot));
  queueHead = 0;
  queueTail = 0;

  squareRoot = sqrt(N3);
  S3 = (int)squareRoot;
  if ((double)(S3 + 1)*(double)(S3 + 1) <= N3)
  {
    S3++;
  }
  if ((double)S3*(double)S3 > N3)
  {
    S3--;
  }
  if ((double)S3*(double)S3 == N3)
  {
    return S3;
  }
  Q13 = 1;
  P3 = S3;
  Q3 = (int)N3 - P3*P3;
  L3 = (int)(2 * sqrt(2 * squareRoot));
  queueHead3 = QUEUE_LENGTH;
  queueTail3 = QUEUE_LENGTH;

  /* Step 2: Cycle forward to find a proper square form */
  for (ctr = 0; ctr <= L; ctr++)
  {
    /* Multiplier == 1 */
    
    // Step 2a for i odd
    q = (S + P) / Q;
    P1 = q*Q - P;
    
    // Step 2b for i odd
    if (Q <= L)
    {
      if ((Q & 1) == 0)
      {
        queue[queueHead] = Q >> 1;
        queueHead++;
        queue[queueHead] = P % (Q >> 1);
        queueHead++;
        if (queueHead == QUEUE_LENGTH)
        {
          queueHead = 0;
        }
      }
      else if (Q + Q <= L)
      {
        queue[queueHead] = Q;
        queueHead++;
        queue[queueHead] = P % Q;
        queueHead++;
        if (queueHead == QUEUE_LENGTH)
        {
          queueHead = 0;
        }
      }
    }
    
       // Step 2c for i odd
    t = Q1 + q*(P - P1);
    Q1 = Q;
    Q = t;
    P = P1;
    
        // Step 2d for i even.
    r = (int)sqrt(Q);
    if (r*r == Q)
    {
      queueIndex = queueTail;
      for (;;)
      {
        if (queueIndex == queueHead)
        {
          /* Step 3: Compute inverse square root of the square form */
          PRev = P;
          Q1Rev = r;
          uRev = (S - PRev) / r;
          PRev += r*uRev;
          QRev = (int)((N - (double)PRev*(double)PRev) / Q1Rev);
          /* Step 4: Cycle in the reverse direction to find a factor of N */
          for (j = ctr; j >= 0; j--)
          {
            // Step 4a
            qRev = (S + PRev) / QRev;
            P1Rev = qRev*QRev - PRev;
            
            // Step 4b
            if (PRev == P1Rev)
            {
              /* Step 5: Get the factor of N */
              if ((QRev & 1) == 0)
              {
                return QRev >> 1;
              }
              return QRev;
            }
            
            // Step 4c
            tRev = Q1Rev + qRev*(PRev - P1Rev);
            Q1Rev = QRev;
            QRev = tRev;
            PRev = P1Rev;
            
            // Step 4a
            qRev = (S + PRev) / QRev;
            P1Rev = qRev*QRev - PRev;
            
            // Step 4b
            if (PRev == P1Rev)
            {
              /* Step 5: Get the factor of N */
              if ((QRev & 1) == 0)
              {
                return QRev >> 1;
              }
              return QRev;
            }
            // Step 4c
            tRev = Q1Rev + qRev*(PRev - P1Rev);
            Q1Rev = QRev;
            QRev = tRev;
            PRev = P1Rev;
          }
          break;
        }
        s = queue[queueIndex];
        queueIndex++;
        t = queue[queueIndex];
        queueIndex++;
        if (queueIndex == QUEUE_LENGTH)
        {
          queueIndex = 0;
        }
        if ((r == s) && (((P - t) % s) == 0))
        {
          break;
        }
      }
      if (r > 1)
      {  // Delete all entries of queue from beginning including the pair
         // for which (P-t)%s = 0.
        queueTail = queueIndex;
      }
      if (r == 1)
      {
        return 0;
      }
    }
       // Step 2a for i even
    q = (S + P) / Q;
    P1 = q*Q - P;
       // Step 2b for i even
    if (Q <= L)
    {
      if ((Q & 1) == 0)
      {
        queue[queueHead] = Q >> 1;
        queueHead++;
        queue[queueHead] = P % (Q >> 1);
        queueHead++;
        if (queueHead == QUEUE_LENGTH)
        {
          queueHead = 0;
        }
      }
      else if (Q + Q <= L)
      {
        queue[queueHead] = Q;
        queueHead++;
        queue[queueHead] = P % Q;
        queueHead++;
        if (queueHead == QUEUE_LENGTH)
        {
          queueHead = 0;
        }
      }
    }
        // Step 2c for i even
    t = Q1 + q*(P - P1);
    Q1 = Q;
    Q = t;
    P = P1;
    

#if 0
    /* Multiplier == 3 */
    q3 = (S3 + P3) / Q3;
    P13 = q3*Q3 - P3;
    if (Q3 <= L3)
    {
      if ((Q3 & 1) == 0)
      {
        queue[queueHead3] = Q3 >> 1;
        queueHead3++;
        queue[queueHead3] = P3 % (Q3 >> 1);
        queueHead3++;
        if (queueHead3 == 2*QUEUE_LENGTH)
        {
          queueHead3 = QUEUE_LENGTH;
        }
      }
      else if (Q3 + Q3 <= L3)
      {
        queue[queueHead3] = Q3;
        queueHead3++;
        queue[queueHead3] = P3 % Q3;
        queueHead3++;
        if (queueHead3 == 2*QUEUE_LENGTH)
        {
          queueHead3 = QUEUE_LENGTH;
        }
      }
    }
    t3 = Q13 + q3*(P3 - P13);
    Q13 = Q3;
    Q3 = t3;
    P3 = P13;
    {
      r3 = (int)sqrt(Q3);
      if (r3*r3 == Q3)
      {
        queueIndex3 = queueTail3;
        for (;;)
        {
          if (queueIndex3 == queueHead3)
          {
            /* Step 3: Compute inverse square root of the square form */
            PRev = P3;
            Q1Rev = r3;
            uRev = (S3 - PRev) % r3;
            uRev += (uRev >> 31) & r3;
            PRev = S3 - uRev;
            QRev = (int)((N3 - (double)PRev*(double)PRev) / Q1Rev);
            /* Step 4: Cycle in the reverse direction to find a factor of N */
            for (j = i; j >= 0; j--)
            {
              qRev = (S3 + PRev) / QRev;
              P1Rev = qRev*QRev - PRev;
              if ((PRev == P1Rev) && (QRev > 6))
              {
                if (QRev % 3 == 0)
                {
                  QRev /= 3;
                }
                /* Step 5: Get the factor of N */
                if ((QRev & 1) == 0)
                {
                  return QRev >> 1;
                }
                return QRev;
              }
              tRev = Q1Rev + qRev*(PRev - P1Rev);
              Q1Rev = QRev;
              QRev = tRev;
              PRev = P1Rev;
              qRev = (S3 + PRev) / QRev;
              P1Rev = qRev*QRev - PRev;
              if ((PRev == P1Rev) && (QRev > 6))
              {
                if (QRev % 3 == 0)
                {
                  QRev /= 3;
                }
                /* Step 5: Get the factor of N */
                if ((QRev & 1) == 0)
                {
                  return QRev >> 1;
                }
                return QRev;
              }
              tRev = Q1Rev + qRev*(PRev - P1Rev);
              Q1Rev = QRev;
              QRev = tRev;
              PRev = P1Rev;
            }
            break;
          }
          s3 = queue[queueIndex3];
          queueHead3++;
          t3 = queue[queueIndex3];
          queueHead3++;
          if (queueIndex3 == 2*QUEUE_LENGTH)
          {
            queueIndex3 = QUEUE_LENGTH;
          }
          if ((r3 == s3) && ((P3 - t3) % s3 == 0))
          {
            break;
          }
        }
        if (r3 > 1)
        {
          queueTail3 = queueIndex3;
        }
        if (r3 == 1)
        {
          return 0;
        }
      }
    }
    q3 = (S3 + P3) / Q3;
    P13 = q3*Q3 - P3;
    if (Q3 <= L3)
    {
      if ((Q3 & 1) == 0)
      {
        queue[queueHead3] = Q3 >> 1;
        queueHead3++;
        queue[queueHead3] = P3 % (Q3 >> 1);
        queueHead3++;
        if (queueHead3 == 2*QUEUE_LENGTH)
        {
          queueHead3 = QUEUE_LENGTH;
        }
      }
      else if (Q3 + Q3 <= L3)
      {
        queue[queueHead3] = Q3;
        queueHead3++;
        queue[queueHead3] = P3 % Q3;
        queueHead3++;
        if (queueHead3 == 2*QUEUE_LENGTH)
        {
          queueHead3 = QUEUE_LENGTH;
        }
      }
    }
    t3 = Q13 + q3*(P3 - P13);
    Q13 = Q3;
    Q3 = t3;
    P3 = P13;
#endif
  }
  return 0;
}

/* If 2^value mod value = 2, then the value is a probable prime (value odd) */
static unsigned char isProbablePrime(double bValue)
{
  int64_t value = (int64_t)bValue;
  int64_t mask, montgomery2, Pr, Prod0, Prod1, MontDig;
  int N, MontgomeryMultN;
  int64_t BaseHI, BaseLO, valueHI, valueLO;
  int x = N = (int)value; // 2 least significant bits of inverse correct.
  x = x * (2 - N * x);     // 4 least significant bits of inverse correct.
  x = x * (2 - N * x);     // 8 least significant bits of inverse correct.
  x = x * (2 - N * x);     // 16 least significant bits of inverse correct.
  x = x * (2 - N * x);     // 32 least significant bits of inverse correct.
  MontgomeryMultN = (-x) & 0x7FFFFFFF;
  mask = (int64_t)1 << 62;
  montgomery2 = 2 * (mask % value);
  if (montgomery2 >= value)
  {
    montgomery2 -= value;
  }
  BaseHI = (int)(montgomery2 >> 31);
  BaseLO = (int)montgomery2 & 0x7FFFFFFF;
  valueHI = (int)(value >> 31);
  valueLO = (int)value & 0x7FFFFFFF;
  while ((mask & value) == 0)
  {
    mask >>= 1;
  }
  mask >>= 1;
  while (mask > 0)
  {
    /* Square the base */
    Pr = BaseLO * BaseLO;
    MontDig = ((int)Pr * MontgomeryMultN) & 0x7FFFFFFFL;
    Prod0 = (Pr = ((MontDig * valueLO + Pr) >> 31) +
      MontDig * valueHI + BaseLO * BaseHI) & 0x7FFFFFFFL;
    Prod1 = Pr >> 31;
    Pr = BaseHI * BaseLO + Prod0;
    MontDig = ((int)Pr * MontgomeryMultN) & 0x7FFFFFFFL;
    Prod0 = (Pr = ((MontDig * valueLO + Pr) >> 31) +
      MontDig * valueHI + BaseHI * BaseHI + Prod1) & 0x7FFFFFFFL;
    Prod1 = Pr >> 31;
    if ((Prod1 > valueHI) || ((Prod1 == valueHI) && (Prod0 >= valueLO)))
    {
      Prod0 = (Pr = Prod0 - valueLO) & 0x7FFFFFFFL;
      Prod1 = ((Pr >> 31) + Prod1 - valueHI) & 0x7FFFFFFFL;
    }
    BaseLO = Prod0;
    BaseHI = Prod1;

    if ((mask & value) != 0)
    {
      /* Multiply by 2 */
      Pr = 2 * ((BaseHI << 31) + BaseLO);
      if (Pr >= value)
      {
        Pr -= value;
      }
      BaseHI = (int)(Pr >> 31);
      BaseLO = (int)Pr & 0x7FFFFFFF;
    }
    mask >>= 1;
  }
  Pr = (BaseHI << 31) + BaseLO;
  return Pr == montgomery2;
}
#endif

