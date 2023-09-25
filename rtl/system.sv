
module system
(
	input   clk,
	input   reset,
	
	input   pal,
	input   scandouble,

	output  ce_pixel,

	output  HBlank,
	output  HSync,
	output  VBlank,
	output  VSync,

	output [7:0] r,
	output [7:0] g,
	output [7:0] b,

	input [15:0] gamepad,
	input [6:0] user_in,
	output reg [6:0] user_out,

    input ioctl_download,
    input [15:0] ioctl_index,
    input        ioctl_wr,
    input [26:0] ioctl_addr,
    input [15:0] ioctl_dout,

	inout [35:0] EXT_BUS,

	output reg    vio_en,
	output reg    vio_strobe,
	input  [15:0] vio_dout,
	output reg [15:0] vio_din,
	input  [15:0] vio_cfg
);

reg phi1, phi2;
reg [31:0] ticks, ticks_latch;

always_ff @(posedge clk) begin
	phi1 <= ~phi1;
	phi2 <= phi1;
end

wire cpu_rw;
wire cpu_read = cpu_rw & &cpu_ds_n;
wire cpu_write = ~cpu_rw & &cpu_ds_n;
wire [1:0] cpu_ds_n;
wire cpu_as_n;
wire [2:0] cpu_fc;

wire ram_sel = cpu_addr[23:16] == 8'h10;
wire ticks_sel = cpu_addr[23:16] == 8'h20;
wire user_sel = cpu_addr[23:16] == 8'h30;
wire pad_sel = cpu_addr[23:16] == 8'h40;
wire hps_sel = cpu_addr[23:16] == 8'h50;
wire hps_valid_sel = cpu_addr[23:16] == 8'h51;
wire crtc_sel = cpu_addr[23:16] == 8'h80;
wire tilemap_ram_sel = cpu_addr[23:16] == 8'h90;
wire tilemap_reg_sel = cpu_addr[23:16] == 8'h91;
wire tilemap_sel = tilemap_ram_sel | tilemap_reg_sel;
wire pal_sel = cpu_addr[23:16] == 8'h92;
wire vio_sel = cpu_addr[23:16] == 8'h60;
wire a1 = cpu_addr[1];

wire [15:0] cpu_din = ram_sel ? ram_dout :
					  crtc_sel ? crtc_dout :
					  tilemap_sel ? tilemap_dout :
					  pal_sel ? pal_dout :
					  (ticks_sel & a1) ? ticks_latch[15:0] :
					  (ticks_sel & ~a1) ? ticks_latch[31:16] :
					  user_sel ? { 9'd0, user_in[6:0] } :
					  pad_sel ? { 1'd0, gamepad } :
					  vio_sel ? vio_dout :
					  rom_dout;

wire [15:0] cpu_dout;
wire [23:0] cpu_addr = { cpu_word_addr, 1'b0 };
wire [22:0] cpu_word_addr;
wire [15:0] rom_dout;
wire [15:0] ram_dout;
wire [15:0] tilemap_dout;
wire [15:0] crtc_dout;
wire [15:0] pal_dout;

reg hps_valid = 0;

reg [1:0] irq_level;
reg [2:0] irq_flagged;

reg cpu_dtack_n;
reg cpu_vpa_n;
reg prev_cpu_ds_n;
wire cpu_ds_posedge = &cpu_ds_n & ~prev_cpu_ds_n;

always_ff @(posedge clk) begin
	if (reset) begin
		cpu_dtack_n <= 1;
		cpu_vpa_n <= 1;
	end else begin
		prev_cpu_ds_n <= &cpu_ds_n;
		if (cpu_as_n || cpu_ds_posedge) begin
			cpu_dtack_n <= 1;
			cpu_vpa_n <= 1;
		end else if (~cpu_as_n) begin
			if (cpu_fc == 3'b111) begin
				cpu_vpa_n <= 0;
			end else begin
				cpu_dtack_n <= 0;
			end
		end
	end
end

always_ff @(posedge clk) begin
	reg prev_strobe;
	if (reset) begin
		ticks <= 32'd0;
		hps_valid <= 0;
		vio_en <= 0;
		vio_strobe <= 0;
		prev_strobe <= 0;
	end else begin
		if (ticks_sel & ~cpu_rw) ticks_latch <= ticks;
		ticks <= ticks + 32'd1;

		if (user_sel & ~cpu_rw & ~cpu_ds_n[0]) user_out <= cpu_dout[6:0];
		if (hps_valid_sel & ~cpu_rw & ~cpu_ds_n[0]) hps_valid <= cpu_dout[0];

		prev_strobe <= 0;
		vio_strobe <= 0;
		if (vio_sel & ~cpu_rw & ~|cpu_ds_n) begin
			if (cpu_addr[15:0] == 16'h8000) begin
				vio_en <= cpu_dout[0];
			end else if (cpu_addr[15:0] == 16'h0000) begin
				prev_strobe <= 1;
				vio_strobe <= ~prev_strobe;
				vio_din <= cpu_dout;
			end
		end
	end
end

reg [2:0] intp_prev;
wire [2:0] intp = { user_in[1], ~VBlank, VBlank };

always_ff @(posedge clk) begin
	if (reset) begin
		irq_level <= 2'b0;
		irq_flagged <= 3'b000;
		intp_prev <= 3'b000;
	end else begin
		if (cpu_fc == 3'b111) begin // acknowledge
			if (irq_level != 2'd0) irq_flagged[irq_level - 1] <= 1'b0;
			irq_level <= 2'd0;
		end else if(irq_level == 3'd0) begin
			if (irq_flagged[2]) irq_level <= 2'd3;
			else if (irq_flagged[1]) irq_level <= 2'd2;
			else if (irq_flagged[0]) irq_level <= 2'd1;
		end

		if (intp[0] & ~intp_prev[0]) irq_flagged[0] <= 1'b1;
		if (intp[1] & ~intp_prev[1]) irq_flagged[1] <= 1'b1;
		if (intp[2] & ~intp_prev[2]) irq_flagged[2] <= 1'b1;
		intp_prev <= intp;
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
	
	.FC0(cpu_fc[0]),
	.FC1(cpu_fc[1]),
	.FC2(cpu_fc[2]),
	.BGn(),
	.oRESETn(),
	.oHALTEDn(),
	.DTACKn(cpu_dtack_n),
	.VPAn(cpu_vpa_n),
	.BERRn(1),
	.BRn(1),
	.BGACKn(1),
	.IPL0n(1),
	.IPL1n(~irq_level[0]),
	.IPL2n(~irq_level[1]),
	.iEdb(cpu_din),
	.oEdb(cpu_dout),
	.eab(cpu_word_addr)
);

singleport_ram #(.widthad(15), .width(16), .name("ROM0"), .init_file("roms/cpu.mif")) cpu_rom(
    .clock(clk),
    .wren(ioctl_download & ioctl_wr),
    .address(ioctl_download ? ioctl_addr[15:1] : cpu_addr[15:1]),
    .data({ioctl_dout[7:0], ioctl_dout[15:8]}),
    .q(rom_dout[15:0])
);

singleport_ram #(.widthad(15), .width(8), .name("RAM0")) cpu_ram_0(
    .clock(clk),
    .wren(ram_sel & ~cpu_rw & ~cpu_ds_n[0]),
    .address(cpu_addr[15:1]),
    .data(cpu_dout[7:0]),
    .q(ram_dout[7:0])
);

singleport_ram #(.widthad(15), .width(8), .name("RAM1")) cpu_ram_1(
    .clock(clk),
    .wren(ram_sel & ~cpu_rw & ~cpu_ds_n[1]),
    .address(cpu_addr[15:1]),
    .data(cpu_dout[15:8]),
    .q(ram_dout[15:8])
);

dualport_unreg_ram #(.width(8), .widthad(8)) pal_ram_0(
    .clock_a(clk),
    .wren_a(pal_sel & ~cpu_rw & cpu_ds_n[0]),
    .address_a(cpu_addr[8:1]),
    .data_a(cpu_dout[7:0]),
    .q_a(pal_dout[7:0]),

    .clock_b(clk),
    .wren_b(0),
    .address_b(color_idx),
    .data_b(),
    .q_b(color_rgb[7:0])
);

dualport_unreg_ram #(.width(8), .widthad(8)) pal_ram_1(
    .clock_a(clk),
    .wren_a(pal_sel & ~cpu_rw & cpu_ds_n[1]),
    .address_a(cpu_addr[8:1]),
    .data_a(cpu_dout[15:8]),
    .q_a(pal_dout[15:8]),

    .clock_b(clk),
    .wren_b(0),
    .address_b(color_idx),
    .data_b(),
    .q_b(color_rgb[15:8])
);

wire [11:0] hcnt, vcnt;

crtc crtc(
    .clk(clk),
    .reset(reset),

    .ce_pixel(ce_pixel),

    .wr((crtc_sel & ~cpu_rw) ? ~cpu_ds_n : 2'b00),

    .address(cpu_addr[4:1]),
    .din(cpu_dout),
    .dout(crtc_dout),

    .hsync(HSync),
    .hblank(HBlank),
    .hcnt(hcnt),

    .vsync(VSync),
    .vblank(VBlank),
    .vcnt(vcnt)
);

wire [7:0] color_idx;
wire [15:0] color_rgb;
assign r = { color_rgb[14:10], color_rgb[14:12] };
assign g = { color_rgb[9:5], color_rgb[9:7] };
assign b = { color_rgb[4:0], color_rgb[4:2] };

tilemap tilemap(
    .clk(clk),
    .reset(reset),

    .ce_pixel(ce_pixel),

    .wr((tilemap_sel & ~cpu_rw) ? ~cpu_ds_n : 2'b00),

	.cs_ram(tilemap_ram_sel),
	.cs_reg(tilemap_reg_sel),

    .address(cpu_addr[16:1]),
    .din(cpu_dout),
    .dout(tilemap_dout),

    .hcnt(hcnt),
    .vcnt(vcnt),

    .color_out(color_idx)
);

hps_ext hps_ext(
	.clk_sys(clk),
	.EXT_BUS(EXT_BUS),
	.valid(hps_valid),
	.wr((hps_sel & ~cpu_rw) ? ~cpu_ds_n : 2'b00),
	.din(cpu_dout),
	.addr(cpu_addr[7:1])
);

endmodule
