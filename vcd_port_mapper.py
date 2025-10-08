#!/usr/bin/env python3
"""
VCD Port Mapper
Extracts port directions from SystemVerilog/Verilog RTL files and updates VCD signal types
"""

import re
import sys
import os
import argparse
from pathlib import Path
import json
import tempfile
import shutil

class VerilogParser:
    """Parser for SystemVerilog and Verilog files to extract module port definitions"""
    
    def __init__(self):
        self.modules = {}
        
    def parse_directory(self, rtl_dir):
        """Parse all Verilog/SystemVerilog files in directory"""
        rtl_path = Path(rtl_dir)
        sv_files = list(rtl_path.glob("**/*.sv")) + list(rtl_path.glob("**/*.v"))
        
        print(f"Found {len(sv_files)} RTL files in {rtl_dir}")
        
        for file_path in sv_files:
            self.parse_file(file_path)
    
    def parse_file(self, file_path):
        """Parse a Verilog/SystemVerilog file and extract module definitions"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Remove comments while preserving line structure
            content = self._remove_comments(content)
            
            self._parse_modules(content, str(file_path))
            
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")
    
    def _remove_comments(self, content):
        """Remove comments from Verilog code"""
        # Remove single-line comments
        content = re.sub(r'//.*', '', content)
        # Remove multi-line comments
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        return content
    
    def _parse_modules(self, content, filename):
        """Extract module definitions from file content"""
        # Pattern to match module definitions (handles ANSI and non-ANSI style)
        module_pattern = r'\bmodule\s+(\w+)\s*(?:#\s*\([^)]*\)\s*)?(?:\s*\([^;]*?\)\s*;|\s*;)'
        
        matches = re.finditer(module_pattern, content, re.DOTALL | re.MULTILINE | re.IGNORECASE)
        
        for match in matches:
            module_name = match.group(1)
            full_match = match.group(0)
            
            # Extract the port list if it exists
            port_list_match = re.search(r'\(\s*([^;]*?)\s*\)\s*;', full_match)
            if port_list_match:
                port_list = port_list_match.group(1)
                ports = self._parse_ansi_style_ports(content, module_name, port_list)
            else:
                # Non-ANSI style - ports declared inside module
                ports = self._parse_non_ansi_style_ports(content, module_name)
            
            if ports:
                self.modules[module_name] = ports
                print(f"Found module: {module_name} with {len(ports)} ports in {filename}")
    
    def _parse_ansi_style_ports(self, content, module_name, port_list):
        """Parse ANSI-style port declarations"""
        ports = {}
        
        # Split port list by commas, handling potential line breaks
        port_names = self._split_port_list(port_list)
        
        # Look for port declarations in the module
        module_section = self._extract_module_section(content, module_name)
        
        # Pattern for ANSI style: direction type [range] port_name
        ansi_pattern = r'(input|output|inout)\s+(?:wire\s+|reg\s+|)?(?:\[[^]]+\]\s+)?(\w+)'
        matches = re.finditer(ansi_pattern, module_section, re.IGNORECASE)
        
        for match in matches:
            direction = match.group(1).lower()
            port_name = match.group(2)
            if port_name in port_names:
                ports[port_name] = direction
        
        return ports
    
    def _parse_non_ansi_style_ports(self, content, module_name):
        """Parse non-ANSI style port declarations"""
        ports = {}
        module_section = self._extract_module_section(content, module_name)
        
        # Pattern for port declarations inside module body
        port_patterns = [
            r'(input|output|inout)\s+(?:wire\s+|reg\s+|)?(?:\[[^]]+\]\s+)?(\w+(?:\s*,\s*\w+)*)',
            r'(input|output|inout)\s+(?:\[[^]]+\]\s+)?(\w+(?:\s*,\s*\w+)*)'
        ]
        
        for pattern in port_patterns:
            matches = re.finditer(pattern, module_section, re.IGNORECASE)
            for match in matches:
                direction = match.group(1).lower()
                port_names = [p.strip() for p in match.group(2).split(',')]
                for port_name in port_names:
                    ports[port_name] = direction
        
        return ports
    
    def _extract_module_section(self, content, module_name):
        """Extract the content of a specific module"""
        # Find module start
        module_start = re.search(r'\bmodule\s+' + module_name + r'\b', content, re.IGNORECASE)
        if not module_start:
            return ""
        
        # Find module end
        remaining = content[module_start.start():]
        brace_count = 0
        in_module = False
        module_content = ""
        
        for char in remaining:
            if char == '{':
                brace_count += 1
                in_module = True
            elif char == '}':
                brace_count -= 1
            
            if in_module:
                module_content += char
            
            if brace_count == 0 and in_module:
                break
        
        return module_content
    
    def _split_port_list(self, port_list):
        """Split port list by commas, handling complex expressions"""
        ports = []
        current = ""
        paren_depth = 0
        
        for char in port_list:
            if char == '(':
                paren_depth += 1
            elif char == ')':
                paren_depth -= 1
            elif char == ',' and paren_depth == 0:
                ports.append(current.strip())
                current = ""
                continue
            current += char
        
        if current.strip():
            ports.append(current.strip())
        
        return ports

class VCDProcessor:
    """Process VCD file to update signal types based on RTL port definitions"""
    
    def __init__(self):
        self.parser = VerilogParser()
    
    def process_vcd(self, vcd_file, rtl_dir, output_file):
        """Process VCD file and update signal types"""
        print(f"Processing VCD: {vcd_file}")
        print(f"RTL Directory: {rtl_dir}")
        print(f"Output VCD: {output_file}")
        
        # Parse RTL files
        self.parser.parse_directory(rtl_dir)
        
        if not self.parser.modules:
            print("No modules found in RTL files")
            return False
        
        # Read VCD file
        with open(vcd_file, 'r', encoding='utf-8', errors='ignore') as f:
            vcd_content = f.read()
        
        # Update VCD content
        updated_content = self._update_vcd_types(vcd_content)
        
        # Write updated VCD
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(updated_content)
        
        print(f"Successfully updated VCD file with port directions")
        return True
    
    def _update_vcd_types(self, vcd_content):
        """Update signal types in VCD content based on RTL port definitions"""
        lines = vcd_content.split('\n')
        updated_lines = []
        
        for line in lines:
            updated_line = self._process_vcd_line(line.strip())
            updated_lines.append(updated_line)
        
        return '\n'.join(updated_lines)
    
    def _process_vcd_line(self, line):
        """Process a single VCD line to update signal types"""
        if line.startswith('$var'):
            # Parse var line: $var type size identifier name $end
            parts = line.split()
            if len(parts) >= 5:
                var_type = parts[1]
                identifier = parts[3]
                full_name = parts[4]
                
                # Extract module and signal name
                module_name, signal_name = self._extract_module_signal(full_name)
                
                if module_name and signal_name and module_name in self.parser.modules:
                    module_ports = self.parser.modules[module_name]
                    if signal_name in module_ports:
                        new_type = module_ports[signal_name]
                        # Replace the type in the var line
                        parts[1] = new_type
                        print(f"Updated {full_name}: {var_type} -> {new_type}")
                        return ' '.join(parts)
        
        return line
    
    def _extract_module_signal(self, full_name):
        """Extract module name and signal name from full hierarchical name"""
        parts = full_name.split('.')
        if len(parts) >= 2:
            # Last part is signal name, second last is module (or instance)
            signal_name = parts[-1]
            module_name = parts[-2]
            return module_name, signal_name
        return None, None

def find_rtl_directory(vcd_file):
    """Find RTL directory relative to VCD file"""
    vcd_path = Path(vcd_file)
    parent_dir = vcd_path.parent
    
    # Check for common RTL directory names
    rtl_dirs = ['rtl', 'RTL', 'src', 'source', 'sources', 'verilog', 'sv']
    
    for rtl_dir in rtl_dirs:
        potential_path = parent_dir / rtl_dir
        if potential_path.exists() and potential_path.is_dir():
            return str(potential_path)
    
    # Check if parent directory itself contains RTL files
    sv_files = list(parent_dir.glob("*.sv")) + list(parent_dir.glob("*.v"))
    if sv_files:
        return str(parent_dir)
    
    return None

def main():
    parser = argparse.ArgumentParser(description='Update VCD signal types from RTL port definitions')
    parser.add_argument('vcd_file', help='Input VCD file')
    parser.add_argument('-o', '--output', help='Output VCD file', required=True)
    parser.add_argument('-r', '--rtl', help='RTL directory (auto-detected if not specified)')
    
    args = parser.parse_args()
    
    # Find RTL directory
    rtl_dir = args.rtl
    if not rtl_dir:
        rtl_dir = find_rtl_directory(args.vcd_file)
    
    if not rtl_dir:
        print("No RTL directory found. Please specify with -r option.")
        return 1
    
    # Process VCD file
    processor = VCDProcessor()
    success = processor.process_vcd(args.vcd_file, rtl_dir, args.output)
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())