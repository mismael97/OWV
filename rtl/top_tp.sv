// Testbench for 4-bit Adder

module tb_four_bit_adder;
  // Testbench signals
  reg [3:0] a, b;
  reg cin;
wire cins;

  wire [3:0] sum;
  wire cout;

wire [3:0] bus_x;
  wire signle_x;

logic [3:0] bus_y;
  logic signle_y;
  

  // Instantiate the 4-bit adder
  four_bit_adder uut (
      .a(a),
      .b(b),
      .cin(cin),
    .cins(cins),
      .sum(sum),
      .cout(cout)
  );

  // Dump file setup for waveform simulation
  initial begin
     $dumpfile("adder_waveform.vcd"); // Specify the dump file name
    $dumpvars(2, tb_four_bit_adder);  // Dump variables up to 1 level to avoid duplicates
  end


  // Test sequence
  initial begin
      $display("Time\ta\tb\tcin\tsum\tcout");
      $monitor("%0d\t%b\t%b\t%b\t%b\t%b", $time, a, b, cin, sum, cout);

      // Test cases
      a = 4'b0000; b = 4'b0000; cin = 0; #10;
      a = 4'b0011; b = 4'b0101; cin = 0; #10;
      a = 4'b1111; b = 4'b0001; cin = 0; #10;
      a = 4'b1010; b = 4'b0101; cin = 1; #10;
      a = 4'b1111; b = 4'b1111; cin = 1; #10;

      $finish;
    
  end
endmodule