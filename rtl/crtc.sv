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
    output [11:0] vcnt
);

reg [15:0] ctrl[16];

wire [9:0] ce_num = ctrl[0][9:0];
wire [9:0] ce_denom = ctrl[1][9:0];

wire [11:0] h_start = ctrl[2][11:0];
wire [11:0] h_end = ctrl[3][11:0];
wire [11:0] hact_end = ctrl[4][11:0];
wire [11:0] hsync_start = ctrl[5][11:0];
wire [11:0] hsync_end = ctrl[6][11:0];

wire [11:0] v_start = ctrl[7][11:0];
wire [11:0] v_end = ctrl[8][11:0];
wire [11:0] vact_end = ctrl[9][11:0];
wire [11:0] vsync_start = ctrl[10][11:0];
wire [11:0] vsync_end = ctrl[11][11:0];


localparam HCNT_REG = 12;
localparam VCNT_REG = 13;

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

assign hblank = hcnt > hact_end || hcnt < h_start;
assign hsync = hcnt > hsync_start && hcnt <= hsync_end;
assign vblank = vcnt > vact_end || vcnt < v_start;
assign vsync = vcnt > vsync_start && vcnt <= vsync_end;
assign dout = ctrl[address];

always_ff @(posedge clk) begin
    if (reset) begin
        ctrl[HCNT_REG] <= 16'd0;
        ctrl[VCNT_REG] <= 16'd0;
    end

    if (wr[0]) ctrl[address][7:0] <= din[7:0];
    if (wr[1]) ctrl[address][15:8] <= din[15:8];

    if (ce_pixel) begin
        ctrl[HCNT_REG] <= hcnt + 12'd1;
        if (hcnt == h_end) begin
            ctrl[HCNT_REG] <= 16'd0;
            ctrl[VCNT_REG] <= vcnt + 12'd1;
            if (vcnt == v_end) begin
                ctrl[VCNT_REG] <= 16'd0;
            end
        end
    end
end

endmodule