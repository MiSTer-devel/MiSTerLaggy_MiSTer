
module mycore
(
	input         clk,
	input         reset,
	
	input         pal,
	input         scandouble,

	output reg    ce_pix,

	output reg    HBlank,
	output reg    HSync,
	output reg    VBlank,
	output reg    VSync,

	output  [7:0] video
);

reg phi1, phi2;

always_ff @(posedge clk) begin
	phi1 <= ~phi1;
	phi2 <= phi1;
end

wire [2:0] irq_level = 3'd0;
wire cpu_rw;
wire cpu_read = cpu_rw & &cpu_ds_n;
wire cpu_write = ~cpu_rw & &cpu_ds_n;
wire [1:0] cpu_ds_n;
wire cpu_as_n;
wire ram_sel = cpu_addr[23:16] == 8'h10;
wire [15:0] cpu_din = ram_sel ? ram_dout : rom_dout;
wire [15:0] cpu_dout;
wire [23:0] cpu_addr = { cpu_word_addr, 1'b0 };
wire [22:0] cpu_word_addr;
wire [15:0] rom_dout;
wire [15:0] ram_dout;

reg cpu_dtack_n;
reg prev_cpu_ds_n;
wire cpu_ds_posedge = &cpu_ds_n & ~prev_cpu_ds_n;

always_ff @(posedge clk) begin
	if (reset) begin
		cpu_dtack_n <= 1;
	end else begin
		prev_cpu_ds_n <= &cpu_ds_n;
		if (cpu_as_n || cpu_ds_posedge) begin
			cpu_dtack_n <= 1;
		end else if (~cpu_as_n) begin
			cpu_dtack_n <= 0;
		end
	end
end

fx68k m68000(
	.clk(clk),
	.HALTn(1),

	.extReset(reset),
	.pwrUp(reset),
	.enPhi1(phi1),
	.enPhi2(phi2),

	.eRWn(cpu_rw),
	.ASn(cpu_as_n),
	.LDSn(cpu_ds_n[0]),
	.UDSn(cpu_ds_n[1]),
	.E(),
	.VMAn(),
	
	.FC0(),
	.FC1(),
	.FC2(),
	.BGn(),
	.oRESETn(),
	.oHALTEDn(),
	.DTACKn(cpu_dtack_n),
	.VPAn(1),
	.BERRn(1),
	.BRn(1),
	.BGACKn(1),
	.IPL0n(~irq_level[0]),
	.IPL1n(~irq_level[1]),
	.IPL2n(~irq_level[2]),
	.iEdb(cpu_din),
	.oEdb(cpu_dout),
	.eab(cpu_word_addr)
);

singleport_ram #(.widthad(15), .width(8), .name("ROM0"), .init_file("cpu_low.hex")) cpu_rom_0(
    .clock(clk),
    .wren(0),
    .address(cpu_addr[15:1]),
    .data(),
    .q(rom_dout[15:8])
);

singleport_ram #(.widthad(15), .width(8), .name("ROM1"), .init_file("cpu_high.hex")) cpu_rom_1(
    .clock(clk),
    .wren(0),
    .address(cpu_addr[15:1]),
    .data(),
    .q(rom_dout[7:0])
);

singleport_ram #(.widthad(15), .width(8), .name("RAM0")) cpu_ram_0(
    .clock(clk),
    .wren(ram_sel & ~cpu_rw),
    .address(cpu_addr[15:1]),
    .data(cpu_dout[15:8]),
    .q(ram_dout[15:8])
);

singleport_ram #(.widthad(15), .width(8), .name("RAM1")) cpu_ram_1(
    .clock(clk),
    .wren(ram_sel & ~cpu_rw),
    .address(cpu_addr[15:1]),
    .data(cpu_dout[7:0]),
    .q(ram_dout[7:0])
);

reg   [9:0] hc;
reg   [9:0] vc;
reg   [9:0] vvc;
reg  [63:0] rnd_reg;

wire  [5:0] rnd_c = {rnd_reg[0],rnd_reg[1],rnd_reg[2],rnd_reg[2],rnd_reg[2],rnd_reg[2]};
wire [63:0] rnd;

lfsr random(rnd);

always @(posedge clk) begin
	if(scandouble) ce_pix <= 1;
		else ce_pix <= ~ce_pix;

	if(reset) begin
		hc <= 0;
		vc <= 0;
	end
	else if(ce_pix) begin
		if(hc == 637) begin
			hc <= 0;
			if(vc == (pal ? (scandouble ? 623 : 311) : (scandouble ? 523 : 261))) begin 
				vc <= 0;
				vvc <= vvc + 9'd6;
			end else begin
				vc <= vc + 1'd1;
			end
		end else begin
			hc <= hc + 1'd1;
		end

		rnd_reg <= rnd;
	end
end

always @(posedge clk) begin
	if (hc == 529) HBlank <= 1;
		else if (hc == 0) HBlank <= 0;

	if (hc == 544) begin
		HSync <= 1;

		if(pal) begin
			if(vc == (scandouble ? 609 : 304)) VSync <= 1;
				else if (vc == (scandouble ? 617 : 308)) VSync <= 0;

			if(vc == (scandouble ? 601 : 300)) VBlank <= 1;
				else if (vc == 0) VBlank <= 0;
		end
		else begin
			if(vc == (scandouble ? 490 : 245)) VSync <= 1;
				else if (vc == (scandouble ? 496 : 248)) VSync <= 0;

			if(vc == (scandouble ? 480 : 240)) VBlank <= 1;
				else if (vc == 0) VBlank <= 0;
		end
	end
	
	if (hc == 590) HSync <= 0;
end

reg  [7:0] cos_out;
wire [5:0] cos_g = cos_out[7:3]+6'd32;
cos cos(vvc + {vc>>scandouble, 2'b00}, cos_out);

assign video = cpu_word_addr[7:0];

endmodule
