module crtc(
    input clk,
    input reset,

    output ce_pixel,

    input [1:0] wr,

    input [3:0] address,
    input [15:0] din,
    output [15:0] dout,

    output hsync,
    output hblank,
    output [11:0] hcnt,

    output vsync,
    output vblank,
    output [11:0] vcnt,

	output [5:0] pll_addr,
	output [31:0] pll_value,
	output reg pll_write,
	input      pll_busy
);

reg [15:0] ctrl[16];

wire [9:0] ce_num = ctrl[0][9:0];
wire [9:0] ce_denom = ctrl[1][9:0];

wire [11:0] hact = ctrl[2][11:0];
wire [11:0] hfp = ctrl[3][11:0];
wire [11:0] hs = ctrl[4][11:0];
wire [11:0] hbp = ctrl[5][11:0];

wire [11:0] vact = ctrl[6][11:0];
wire [11:0] vfp = ctrl[7][11:0];
wire [11:0] vs = ctrl[8][11:0];
wire [11:0] vbp = ctrl[9][11:0];

assign pll_addr = ctrl[12][5:0];
assign pll_value = {ctrl[10], ctrl[11]};

localparam PLL_IO_REG = 13;
localparam HCNT_REG = 14;
localparam VCNT_REG = 15;

assign hcnt = ctrl[HCNT_REG][11:0];
assign vcnt = ctrl[VCNT_REG][11:0];

wire ce_pixel2; // unused
jtframe_frac_cen ce_pix_frac(
    .clk(clk),
    .cen_in(1),
    .n(ce_num),
    .m(ce_denom),
    .cen({ce_pixel2, ce_pixel}),
    .cenb()
);


reg [11:0] hb_start, vb_start;
reg [11:0] hs_start, vs_start;
reg [11:0] hs_end, vs_end;

assign hblank = hcnt >= hb_start || hcnt < hbp;
assign hsync = hcnt >= hs_start && hcnt < hs_end;
assign vblank = vcnt >= vb_start || vcnt < vbp;
assign vsync = vcnt >= vs_start && vcnt < vs_end;
assign dout = address == PLL_IO_REG ? {16{pll_busy}} : ctrl[address];

wire pll_io_req = wr[0] && address == PLL_IO_REG;

always_ff @(posedge clk) begin
    reg pll_io_req_prev;

    if (reset) begin
        ctrl[HCNT_REG] <= 16'd0;
        ctrl[VCNT_REG] <= 16'd0;
    end

    if (wr[0]) ctrl[address][7:0] <= din[7:0];
    if (wr[1]) ctrl[address][15:8] <= din[15:8];

    pll_io_req_prev <= pll_io_req;
    pll_write <= pll_io_req & ~pll_io_req_prev;


    hb_start <= hact + hbp;
    vb_start <= vact + vbp;
    hs_start <= hb_start + hfp;
    vs_start <= vb_start + vfp;
    hs_end <= hs_start + hs;
    vs_end <= vs_start + vs;

    if (ce_pixel) begin
        ctrl[HCNT_REG] <= hcnt + 12'd1;
        if (hcnt >= (hs_end - 1)) begin
            ctrl[HCNT_REG] <= 16'd0;
            ctrl[VCNT_REG] <= vcnt + 12'd1;
            if (vcnt >= (vs_end - 1)) begin
                ctrl[VCNT_REG] <= 16'd0;
            end
        end
    end
end

endmodule