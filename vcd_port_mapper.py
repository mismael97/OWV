#!/usr/bin/env python3
"""
VCD Port Mapper - Dynamic version with automatic instance mapping
"""

import re
import sys
import argparse
from pathlib import Path

class VCDProcessor:
    def __init__(self):
        pass
    
    def extract_vcd_modules(self, vcd_file):
        """Extract modules and instances from VCD file"""
        print("=== EXTRACTING MODULES FROM VCD ===")
        
        modules = set()
        instances = {}  # instance_name -> module_type
        
        with open(vcd_file, 'r') as f:
            lines = f.readlines()
        
        current_module = None
        hierarchy = []
        
        for line in lines:
            line = line.strip()
            
            if line.startswith('$scope module'):
                parts = line.split()
                if len(parts) >= 3:
                    scope_name = parts[2]
                    hierarchy.append(scope_name)
                    current_module = scope_name
                    modules.add(scope_name)
                    print(f"Found module/instance: {scope_name}")
            
            elif line.startswith('$upscope'):
                if hierarchy:
                    hierarchy.pop()
                    current_module = hierarchy[-1] if hierarchy else None
        
        print(f"Modules/instances found: {list(modules)}")
        return modules

    def discover_instance_mapping(self, rtl_dir, vcd_modules):
        """Automatically discover instance-to-module mappings from RTL files"""
        print(f"\n=== DISCOVERING INSTANCE MAPPINGS ===")
        print(f"Looking for RTL files in: {rtl_dir}")
        
        rtl_path = Path(rtl_dir)
        if not rtl_path.exists():
            print(f"ERROR: RTL directory {rtl_dir} does not exist!")
            return {}
        
        # Find all SystemVerilog and Verilog files recursively
        sv_files = list(rtl_path.glob("**/*.sv")) + list(rtl_path.glob("**/*.v"))
        print(f"Found {len(sv_files)} RTL files")
        
        if not sv_files:
            print("ERROR: No RTL files found!")
            return {}
        
        instance_map = {}
        all_rtl_modules = set()
        
        for file_path in sv_files:
            print(f"\nProcessing {file_path}:")
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                # Remove single-line and multi-line comments
                content = re.sub(r'//.*', '', content)
                content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
                
                # Find all module definitions
                module_pattern = r'\bmodule\s+(\w+)\s*[#(;]'
                module_matches = re.finditer(module_pattern, content, re.IGNORECASE)
                
                modules_in_file = []
                for match in module_matches:
                    module_name = match.group(1)
                    all_rtl_modules.add(module_name)
                    modules_in_file.append(module_name)
                
                if modules_in_file:
                    print(f"  Found modules: {modules_in_file}")
                
                # Find module instantiations with better pattern
                # This pattern handles various instantiation styles:
                # module_type #(params) instance_name ( .port(sig) );
                # module_type instance_name ( .port(sig) );
                instantiation_pattern = r'\b(\w+)\s*(?:#\s*\([^)]*\)\s*)?\s*(\w+)\s*\(\s*(?:\.[^;]+)\s*\)\s*;'
                instantiation_matches = re.finditer(instantiation_pattern, content, re.IGNORECASE)
                
                instantiations_in_file = []
                for match in instantiation_matches:
                    module_type = match.group(1)
                    instance_name = match.group(2)
                    
                    # Skip common keywords that are not modules
                    skip_keywords = ['if', 'else', 'begin', 'end', 'case', 'generate', 'initial', 
                                   'always', 'assign', 'function', 'task', 'for', 'while', 'repeat',
                                   'forever', 'wait', 'disable', 'assert', 'assume', 'cover',
                                   'property', 'sequence', 'logic', 'reg', 'wire', 'input', 'output',
                                   'inout', 'parameter', 'localparam', 'typedef', 'struct', 'enum',
                                   'package', 'class', 'program', 'interface', 'modport', 'clocking']
                    
                    if module_type.lower() in [kw.lower() for kw in skip_keywords]:
                        continue
                        
                    instantiations_in_file.append((module_type, instance_name))
                    
                    # Only map if the module type exists in RTL and instance exists in VCD
                    if module_type in all_rtl_modules and instance_name in vcd_modules:
                        instance_map[instance_name] = module_type
                        print(f"  Discovered mapping: {instance_name} -> {module_type}")
                
                if instantiations_in_file:
                    print(f"  Found instantiations: {instantiations_in_file}")
                    
            except Exception as e:
                print(f"  ERROR reading {file_path}: {e}")
        
        # For modules that are not instances (direct matches)
        print(f"\nChecking for direct module matches...")
        for vcd_module in vcd_modules:
            if vcd_module in all_rtl_modules and vcd_module not in instance_map:
                instance_map[vcd_module] = vcd_module
                print(f"Direct match: {vcd_module} -> {vcd_module}")
        
        print(f"\nFinal instance map: {instance_map}")
        return instance_map

    def find_rtl_modules(self, rtl_dir, vcd_modules, instance_map):
        """Find modules in RTL files with accurate port extraction"""
        print(f"\n=== FINDING MODULES IN RTL ===")
        print(f"VCD modules/instances: {list(vcd_modules)}")
        print(f"Instance mappings: {instance_map}")
        
        # What we actually need to find in RTL
        target_modules = set(instance_map.values())
        
        print(f"Looking for RTL modules: {list(target_modules)}")
        
        rtl_path = Path(rtl_dir)
        sv_files = list(rtl_path.glob("**/*.sv")) + list(rtl_path.glob("**/*.v"))
        
        print(f"RTL files found: {len(sv_files)} files")
        
        found_modules = {}
        
        for file_path in sv_files:
            print(f"\nChecking {file_path.name}:")
            
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                # Remove comments
                content = re.sub(r'//.*', '', content)
                content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
                
                # Find ALL module definitions in this file
                module_pattern = r'\bmodule\s+(\w+)\s*[#(;]'
                matches = re.finditer(module_pattern, content, re.IGNORECASE)
                
                for match in matches:
                    module_name = match.group(1)
                    print(f"  Found module: {module_name}")
                    
                    if module_name in target_modules:
                        print(f"  [PROCESSING] {module_name}")
                        
                        # Extract ports using simple and accurate method
                        ports = self._extract_ports_simple(content, module_name)
                        
                        if ports:
                            found_modules[module_name] = ports
                            print(f"    Ports found: {len(ports)}")
                            for port_name, direction in ports.items():
                                print(f"      {port_name}: {direction}")
                        else:
                            print(f"    [WARNING] No ports found for {module_name}")
                    else:
                        print(f"  [SKIP] {module_name} (not in target)")
            except Exception as e:
                print(f"  ERROR processing {file_path}: {e}")
        
        return found_modules

    def _extract_ports_simple(self, content, module_name):
        """Extract ports using simple line-by-line parsing"""
        ports = {}
        
        # Find the module section
        module_start = re.search(r'\bmodule\s+' + module_name + r'\b', content, re.IGNORECASE)
        if not module_start:
            return ports
        
        # Extract module content
        remaining = content[module_start.start():]
        endmodule_pos = remaining.find('endmodule')
        if endmodule_pos == -1:
            return ports
        
        module_content = remaining[:endmodule_pos]
        
        # Split into lines and process each line
        lines = module_content.split('\n')
        
        for line in lines:
            line = line.strip()
            
            # Skip empty lines and comments
            if not line or line.startswith('//'):
                continue
            
            # Look for port declarations - SIMPLE pattern
            # Match: direction type [range] port_name
            port_pattern = r'\b(input|output|inout)\s+(?:wire\s+|reg\s+|logic\s+|)\s*(?:\[[^]]+\]\s+)*\s*(\w+)\s*(?:,|;|$)'
            match = re.search(port_pattern, line, re.IGNORECASE)
            
            if match:
                direction = match.group(1).lower()
                port_name = match.group(2)
                
                # Only add if it's a valid port name (not a keyword)
                if port_name not in ['wire', 'reg', 'logic', 'input', 'output', 'inout']:
                    ports[port_name] = direction
                    continue
            
            # Also look for port declarations with multiple ports on one line
            multi_port_pattern = r'\b(input|output|inout)\s+(?:wire\s+|reg\s+|logic\s+|)\s*(?:\[[^]]+\]\s+)*\s*([\w\s,]+)'
            multi_match = re.search(multi_port_pattern, line, re.IGNORECASE)
            
            if multi_match:
                direction = multi_match.group(1).lower()
                port_names_str = multi_match.group(2)
                
                # Split by commas and clean up
                port_names = [p.strip() for p in port_names_str.split(',')]
                for port_name in port_names:
                    # Remove any trailing semicolons or other characters
                    port_name = re.sub(r'[;,].*', '', port_name).strip()
                    if port_name and port_name not in ['wire', 'reg', 'logic', 'input', 'output', 'inout']:
                        ports[port_name] = direction
        
        return ports

    def update_vcd_file(self, vcd_file, output_file, rtl_modules, instance_map):
        """Update VCD file with port information"""
        print(f"\n=== UPDATING VCD FILE ===")
        
        with open(vcd_file, 'r') as f:
            lines = f.readlines()
        
        updated_lines = []
        current_module = None
        update_count = 0
        
        for line in lines:
            stripped = line.strip()
            
            # Track current module
            if stripped.startswith('$scope module'):
                parts = stripped.split()
                if len(parts) >= 3:
                    current_module = parts[2]
                    print(f"Entering module: {current_module}")
            
            # Update var lines
            elif stripped.startswith('$var') and current_module:
                parts = stripped.split()
                if len(parts) >= 5:
                    current_type = parts[1]
                    signal_name = parts[4]
                    
                    # Map instance to actual module
                    actual_module = instance_map.get(current_module, current_module)
                    
                    if actual_module in rtl_modules:
                        ports = rtl_modules[actual_module]
                        if signal_name in ports:
                            new_type = ports[signal_name]
                            parts[1] = new_type
                            updated_line = ' '.join(parts)
                            print(f"UPDATE: {current_module}.{signal_name}: {current_type} -> {new_type}")
                            updated_lines.append(updated_line + '\n')
                            update_count += 1
                            continue
                    else:
                        print(f"SKIP: {current_module}.{signal_name} - Module {actual_module} not found in RTL")
            
            # Exit scope
            elif stripped.startswith('$upscope'):
                current_module = None
            
            updated_lines.append(line)
        
        # Write output
        with open(output_file, 'w') as f:
            f.writelines(updated_lines)
        
        print(f"\n=== SUMMARY ===")
        print(f"Updated {update_count} signal types")
        print(f"Output file: {output_file}")
        
        return update_count > 0

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('vcd_file')
    parser.add_argument('-o', '--output', required=True)
    parser.add_argument('-r', '--rtl', required=True)
    
    args = parser.parse_args()
    
    processor = VCDProcessor()
    
    # Step 1: Extract modules from VCD
    vcd_modules = processor.extract_vcd_modules(args.vcd_file)
    if not vcd_modules:
        print("ERROR: No modules found in VCD")
        return 1
    
    # Step 2: Discover instance mappings automatically
    instance_map = processor.discover_instance_mapping(args.rtl, vcd_modules)
    if not instance_map:
        print("ERROR: No instance mappings discovered")
        return 1
    
    # Step 3: Find modules in RTL
    rtl_modules = processor.find_rtl_modules(args.rtl, vcd_modules, instance_map)
    if not rtl_modules:
        print("ERROR: No matching modules found in RTL")
        return 1
    
    print(f"\nFound RTL modules: {list(rtl_modules.keys())}")
    
    # Step 4: Update VCD file
    success = processor.update_vcd_file(args.vcd_file, args.output, rtl_modules, instance_map)
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())