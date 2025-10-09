// 4-bit Adder Module

module four_bit_adder (
  input logic [3:0] a,      // 4-bit input A
  input logic [3:0] b,      // 4-bit input B
  input logic cin,          // Carry-in
  inout wire cins,          // Carry-in
  output logic [3:0] sum,   // 4-bit Sum output
  output logic cout         // Carry-out
);
logic [2:0] empty;
 

assign {cout, sum} = a + b + cin;
 

endmodule