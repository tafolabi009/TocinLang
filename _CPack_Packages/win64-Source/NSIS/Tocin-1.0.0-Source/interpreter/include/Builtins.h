#ifndef BUILTINS_H
#define BUILTINS_H

#include "Runtime.h"
#include <functional>
#include <unordered_map>
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cmath>
#include <complex>
#include <numeric>

namespace interpreter {

    using BuiltinFunction = std::function<Value(const std::vector<Value>&)>;

    class Builtins {
    public:
        static void registerBuiltins(std::unordered_map<std::string, BuiltinFunction>& builtins);

    private:
        // String operations
        static Value print(const std::vector<Value>& args);
        static Value strLength(const std::vector<Value>& args);
        static Value strConcat(const std::vector<Value>& args);
        static Value strSlice(const std::vector<Value>& args);
        static Value strSplit(const std::vector<Value>& args);
        static Value strJoin(const std::vector<Value>& args);
        static Value strTrim(const std::vector<Value>& args);
        static Value strToUpper(const std::vector<Value>& args);
        static Value strToLower(const std::vector<Value>& args);
        static Value strReplace(const std::vector<Value>& args);
        static Value strContains(const std::vector<Value>& args);
        static Value strStartsWith(const std::vector<Value>& args);
        static Value strEndsWith(const std::vector<Value>& args);

        // Math operations
        static Value mathSqrt(const std::vector<Value>& args);
        static Value mathPow(const std::vector<Value>& args);
        static Value mathSin(const std::vector<Value>& args);
        static Value mathCos(const std::vector<Value>& args);
        static Value mathTan(const std::vector<Value>& args);
        static Value mathLog(const std::vector<Value>& args);
        static Value mathExp(const std::vector<Value>& args);
        static Value mathAbs(const std::vector<Value>& args);
        static Value mathFloor(const std::vector<Value>& args);
        static Value mathCeil(const std::vector<Value>& args);
        static Value mathRound(const std::vector<Value>& args);
        static Value mathRandom(const std::vector<Value>& args);

        // Array operations
        static Value arrayLength(const std::vector<Value>& args);
        static Value arrayPush(const std::vector<Value>& args);
        static Value arrayPop(const std::vector<Value>& args);
        static Value arrayShift(const std::vector<Value>& args);
        static Value arrayUnshift(const std::vector<Value>& args);
        static Value arraySlice(const std::vector<Value>& args);
        static Value arrayConcat(const std::vector<Value>& args);
        static Value arrayReverse(const std::vector<Value>& args);
        static Value arraySort(const std::vector<Value>& args);
        static Value arrayFilter(const std::vector<Value>& args);
        static Value arrayMap(const std::vector<Value>& args);
        static Value arrayReduce(const std::vector<Value>& args);

        // Dictionary operations
        static Value dictKeys(const std::vector<Value>& args);
        static Value dictValues(const std::vector<Value>& args);
        static Value dictHasKey(const std::vector<Value>& args);
        static Value dictGet(const std::vector<Value>& args);
        static Value dictSet(const std::vector<Value>& args);
        static Value dictDelete(const std::vector<Value>& args);
        static Value dictMerge(const std::vector<Value>& args);

        // System operations
        static Value timeNow(const std::vector<Value>& args);
        static Value timeSleep(const std::vector<Value>& args);
        static Value systemExit(const std::vector<Value>& args);
        static Value systemEnv(const std::vector<Value>& args);
        static Value systemCwd(const std::vector<Value>& args);
        static Value systemExec(const std::vector<Value>& args);

        // Type conversion
        static Value toInt(const std::vector<Value>& args);
        static Value toFloat(const std::vector<Value>& args);
        static Value toString(const std::vector<Value>& args);
        static Value toBool(const std::vector<Value>& args);
        static Value toArray(const std::vector<Value>& args);
        static Value toDict(const std::vector<Value>& args);

        // Regular Expression Operations
        static Value regexMatch(const std::vector<Value>& args);
        static Value regexReplace(const std::vector<Value>& args);
        static Value regexSplit(const std::vector<Value>& args);
        static Value regexFindAll(const std::vector<Value>& args);
        static Value regexEscape(const std::vector<Value>& args);

        // Advanced Math Operations
        static Value mathComplex(const std::vector<Value>& args);
        static Value mathConjugate(const std::vector<Value>& args);
        static Value mathNorm(const std::vector<Value>& args);
        static Value mathPhase(const std::vector<Value>& args);
        static Value mathPolar(const std::vector<Value>& args);
        static Value mathRect(const std::vector<Value>& args);
        static Value mathGCD(const std::vector<Value>& args);
        static Value mathLCM(const std::vector<Value>& args);
        static Value mathFactorial(const std::vector<Value>& args);
        static Value mathCombinations(const std::vector<Value>& args);
        static Value mathPermutations(const std::vector<Value>& args);

        // Statistics Operations
        static Value statsMean(const std::vector<Value>& args);
        static Value statsMedian(const std::vector<Value>& args);
        static Value statsMode(const std::vector<Value>& args);
        static Value statsVariance(const std::vector<Value>& args);
        static Value statsStdDev(const std::vector<Value>& args);
        static Value statsCorrelation(const std::vector<Value>& args);
        static Value statsLinearRegression(const std::vector<Value>& args);
        static Value statsQuantile(const std::vector<Value>& args);
        static Value statsHistogram(const std::vector<Value>& args);

        // File System Operations
        static Value fileRead(const std::vector<Value>& args);
        static Value fileWrite(const std::vector<Value>& args);
        static Value fileAppend(const std::vector<Value>& args);
        static Value fileExists(const std::vector<Value>& args);
        static Value fileDelete(const std::vector<Value>& args);
        static Value fileCopy(const std::vector<Value>& args);
        static Value fileMove(const std::vector<Value>& args);
        static Value fileSize(const std::vector<Value>& args);
        static Value fileModified(const std::vector<Value>& args);
        static Value fileList(const std::vector<Value>& args);

        // Network Operations
        static Value netHttpGet(const std::vector<Value>& args);
        static Value netHttpPost(const std::vector<Value>& args);
        static Value netHttpPut(const std::vector<Value>& args);
        static Value netHttpDelete(const std::vector<Value>& args);
        static Value netSocketCreate(const std::vector<Value>& args);
        static Value netSocketConnect(const std::vector<Value>& args);
        static Value netSocketSend(const std::vector<Value>& args);
        static Value netSocketReceive(const std::vector<Value>& args);
        static Value netSocketClose(const std::vector<Value>& args);

        // Cryptography Operations
        static Value cryptoHash(const std::vector<Value>& args);
        static Value cryptoEncrypt(const std::vector<Value>& args);
        static Value cryptoDecrypt(const std::vector<Value>& args);
        static Value cryptoGenerateKey(const std::vector<Value>& args);
        static Value cryptoSign(const std::vector<Value>& args);
        static Value cryptoVerify(const std::vector<Value>& args);
        static Value cryptoRandomBytes(const std::vector<Value>& args);
        static Value cryptoPBKDF2(const std::vector<Value>& args);

        // Date and Time Operations
        static Value dateNow(const std::vector<Value>& args);
        static Value dateParse(const std::vector<Value>& args);
        static Value dateFormat(const std::vector<Value>& args);
        static Value dateAdd(const std::vector<Value>& args);
        static Value dateSubtract(const std::vector<Value>& args);
        static Value dateDiff(const std::vector<Value>& args);
        static Value dateIsValid(const std::vector<Value>& args);
        static Value dateGetTimezone(const std::vector<Value>& args);
        static Value dateSetTimezone(const std::vector<Value>& args);

        // JSON Operations
        static Value jsonParse(const std::vector<Value>& args);
        static Value jsonStringify(const std::vector<Value>& args);
        static Value jsonValidate(const std::vector<Value>& args);
        static Value jsonGet(const std::vector<Value>& args);
        static Value jsonSet(const std::vector<Value>& args);
        static Value jsonDelete(const std::vector<Value>& args);
        static Value jsonMerge(const std::vector<Value>& args);
        static Value jsonPatch(const std::vector<Value>& args);
    };

} // namespace interpreter

#endif // BUILTINS_H 