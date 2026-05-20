import os
import argparse

def generate(num_files, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    
    makefile_path = os.path.join(out_dir, "Makefile")
    
    with open(makefile_path, "w") as f:
        f.write(".PHONY: all clean\n\n")
        f.write("all: final.out\n\n")
        
        objs = [f"obj_{i}.o" for i in range(num_files)]
        objs.append("obj_main.o")
        f.write(f"final.out: {' '.join(objs)}\n")
        f.write(f"\tCCACHE_DISABLE=1 g++ {' '.join(objs)} -o final.out\n\n")
        
        for i in range(num_files):
            cpp_file = f"src_{i}.cpp"
            f.write(f"obj_{i}.o: {cpp_file}\n")
            f.write(f"\tCCACHE_DISABLE=1 g++ -c {cpp_file} -o obj_{i}.o -O2\n\n")
            
            # Generate the cpp file that includes bits/stdc++.h
            with open(os.path.join(out_dir, cpp_file), "w") as cpp:
                cpp.write("#include <bits/stdc++.h>\n")
                cpp.write("using namespace std;\n")
                cpp.write(f"int function_{i}() {{\n")
                cpp.write(f"    vector<int> v(100, {i});\n")
                cpp.write(f"    return accumulate(v.begin(), v.end(), 0);\n")
                cpp.write("}\n")
                
        # create a main.cpp
        f.write("obj_main.o: main.cpp\n")
        f.write("\tCCACHE_DISABLE=1 g++ -c main.cpp -o obj_main.o -O2\n\n")
        
        with open(os.path.join(out_dir, "main.cpp"), "w") as cpp:
            cpp.write("#include <iostream>\n")
            for i in range(num_files):
                cpp.write(f"int function_{i}();\n")
            cpp.write("int main() {\n")
            cpp.write("    int total = 0;\n")
            for i in range(num_files):
                cpp.write(f"    total += function_{i}();\n")
            cpp.write("    std::cout << \"Total: \" << total << \"\\n\";\n")
            cpp.write("    return 0;\n")
            cpp.write("}\n")
            
        f.write("clean:\n")
        f.write("\trm -f *.o *.out\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--files", type=int, default=50, help="Number of dummy files")
    parser.add_argument("--dir", type=str, default="test_build", help="Output directory")
    args = parser.parse_args()
    generate(args.files, args.dir)
    print(f"Generated {args.files} real C++ files in {args.dir}")
