//
// hps_ext for ao486
//
// Copyright (c) 2020 Alexey Melnikov
//
// This source file is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////

module hps_ext
(
	input        clk_sys,
	inout [35:0] EXT_BUS,

	input        valid,
	input [1:0]  wr,

	input [15:0] din,
	input [6:0]  addr
);

assign EXT_BUS[15:0] = io_dout;
wire [15:0] io_din = EXT_BUS[31:16];
assign EXT_BUS[32] = dout_en;
wire io_strobe = EXT_BUS[33];
wire io_enable = |EXT_BUS[35:34];

localparam EXT_CMD_MIN = 'h70;
localparam EXT_CMD_MAX = 'h70;

reg [15:0] io_dout;
reg        dout_en = 0;
reg  [7:0] word_cnt;

reg [15:0] version = 0;
reg [7:0] data_lo[128];
reg [7:0] data_hi[128];
reg prev_valid = 0;

always@(posedge clk_sys) begin
	prev_valid <= valid;

	if (valid & ~prev_valid) begin
		version <= version + 16'd1;
	end

	if (wr[0]) data_lo[addr] <= din[7:0];
	if (wr[1]) data_hi[addr] <= din[15:8];

	if(~io_enable) begin
		word_cnt <= 0;
		io_dout <= 0;
		dout_en <= 0;
	end	else begin
		if(io_strobe) begin
			io_dout <= 0;
			word_cnt <= word_cnt + 1'd1;

			if(word_cnt == 0) begin
				dout_en <= (io_din == 'h70);
				io_dout <= valid ? { 1'b1, version[14:0] } : 16'd0;
			end else begin
				io_dout <= { data_lo[word_cnt - 8'd1], data_hi[word_cnt - 8'd1] };
			end
		end
	end
end

endmodule
