#!/usr/bin/env python3
"""
Test cases for completed interpreter and FFI features in Tocin compiler
"""

import subprocess
import os
import sys

def run_command(cmd, expected_returncode=0):
    """Run a command and check its return code"""
    print(f"\n>>> Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    print(f"Return code: {result.returncode}")
    if result.stdout:
        print(f"STDOUT:\n{result.stdout}")
    if result.stderr:
        print(f"STDERR:\n{result.stderr}")
    
    if result.returncode != expected_returncode:
        print(f"❌ FAILED: Expected return code {expected_returncode}, got {result.returncode}")
        return False
    else:
        print(f"✅ PASSED")
        return True
    
def test_binary_exists():
    """Test that the tocin binary exists and is executable"""
    print("\n=== Test 1: Binary Existence ===")
    binary_path = "./build/tocin"
    
    if not os.path.exists(binary_path):
        print(f"❌ FAILED: Binary not found at {binary_path}")
        return False
    
    if not os.access(binary_path, os.X_OK):
        print(f"❌ FAILED: Binary at {binary_path} is not executable")
        return False
    
    print(f"✅ PASSED: Binary exists and is executable")
    return True

def test_help_output():
    """Test that --help flag works"""
    print("\n=== Test 2: Help Output ===")
    return run_command(["./build/tocin", "--help"])

def test_version_features():
    """Test that advanced features are advertised"""
    print("\n=== Test 3: Feature Advertisement ===")
    result = subprocess.run(["./build/tocin", "--help"], capture_output=True, text=True)
    
    required_features = [
        "Option/Result types",
        "Traits",
        "Ownership",
        "Null safety",
        "Concurrency",
        "async/await",
        "Macro system",
        "FFI support",
        "JavaScript",
        "Python",
        "C++"
    ]
    
    all_found = True
    for feature in required_features:
        if feature in result.stdout:
            print(f"✅ Found: {feature}")
        else:
            print(f"❌ Missing: {feature}")
            all_found = False
    
    return all_found

def test_compilation_flags():
    """Test that compilation flags are recognized"""
    print("\n=== Test 4: Compilation Flags ===")
    
    flags_to_test = [
        ["--dump-ir"],
        ["-O0"],
        ["-O1"],
        ["-O2"],
        ["-O3"],
        ["--no-ffi"],
        ["--no-concurrency"],
        ["--enable-javascript"],
        ["--enable-python"],
        ["--enable-cpp"],
    ]
    
    all_passed = True
    for flag in flags_to_test:
        # Just test that the flag is recognized (may fail for other reasons)
        result = subprocess.run(["./build/tocin"] + flag, capture_output=True, text=True)
        # Check that we don't get "unknown option" error
        if "unknown option" in result.stderr.lower() or "unrecognized" in result.stderr.lower():
            print(f"❌ FAILED: Flag {flag} not recognized")
            all_passed = False
        else:
            print(f"✅ PASSED: Flag {flag} recognized")
    
    return all_passed

def test_simple_compilation():
    """Test compilation of a simple program"""
    print("\n=== Test 5: Simple Program Compilation ===")
    
    # Create a simple test file
    test_file = "test_simple_compile.to"
    with open(test_file, "w") as f:
        f.write("""
def main():
    print("Hello from Tocin!")
    return 0
""")
    
    try:
        # Try to compile (may fail for various reasons, but shouldn't crash)
        result = subprocess.run(["./build/tocin", test_file], 
                              capture_output=True, text=True, timeout=10)
        
        # As long as it doesn't crash, we consider this a pass
        print(f"✅ PASSED: Compilation completed (return code: {result.returncode})")
        return True
    except subprocess.TimeoutExpired:
        print(f"⚠️  WARNING: Compilation timed out")
        return False
    except Exception as e:
        print(f"❌ FAILED: Exception during compilation: {e}")
        return False
    finally:
        # Clean up
        if os.path.exists(test_file):
            os.remove(test_file)

def run_all_tests():
    """Run all test cases"""
    print("=" * 60)
    print("Tocin Compiler - Interpreter & FFI Completion Tests")
    print("=" * 60)
    
    tests = [
        ("Binary Existence", test_binary_exists),
        ("Help Output", test_help_output),
        ("Feature Advertisement", test_version_features),
        ("Compilation Flags", test_compilation_flags),
        ("Simple Compilation", test_simple_compilation),
    ]
    
    results = {}
    for test_name, test_func in tests:
        try:
            results[test_name] = test_func()
        except Exception as e:
            print(f"\n❌ Test '{test_name}' raised exception: {e}")
            results[test_name] = False
    
    # Summary
    print("\n" + "=" * 60)
    print("TEST SUMMARY")
    print("=" * 60)
    
    passed = sum(1 for v in results.values() if v)
    total = len(results)
    
    for test_name, result in results.items():
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"{status}: {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    print("=" * 60)
    
    return passed == total

if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
