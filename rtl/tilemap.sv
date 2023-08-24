module tilemap(
    input clk,
    input reset,

    input ce_pixel,

    input [1:0] wr,

    input cs_ram,
    input cs_reg,

    input [15:0] address,
    input [15:0] din,
    output [15:0] dout,

    input [11:0] hcnt,
    input [11:0] vcnt,

    output reg [7:0] color_out
);

reg [15:0] hofs;
reg [15:0] vofs;

wire [11:0] H = hcnt + hofs[11:0];
wire [11:0] V = vcnt + vofs[11:0];

wire [15:0] tileref_q;

wire [15:0] ram_dout;

assign dout = (cs_reg & ~address[0]) ? hofs : (cs_reg & address[0]) ? vofs : ram_dout;

dualport_ram #(.width(8), .widthad(14)) ram_0
(
    .clock_a(clk),
    .wren_a(wr[0] & cs_ram),
    .address_a(address[13:0]),
    .data_a(din[7:0]),
    .q_a(ram_dout[7:0]),

    .clock_b(clk),
    .wren_b(0),
    .address_b({V[9:3], H[9:3]}),
    .data_b(0),
    .q_b(tileref_q[7:0])
);

dualport_ram #(.width(8), .widthad(14)) ram_1
(
    .clock_a(clk),
    .wren_a(wr[1] & cs_ram),
    .address_a(address[13:0]),
    .data_a(din[15:8]),
    .q_a(ram_dout[15:8]),

    .clock_b(clk),
    .wren_b(0),
    .address_b({V[9:3], H[9:3]}),
    .data_b(0),
    .q_b(tileref_q[15:8])
);

singleport_ram #(.widthad(13), .width(32), .name("GFX"), .init_file("roms/gfx.mif")) gfx_rom(
    .clock(clk),
    .wren(0),
    .address({tileref[9:0], V[2:0]}),
    .data(),
    .q(gfx_data)
);

reg [11:0] vcnt_prev;
reg [15:0] tileref;
wire [31:0] gfx_data;
reg [31:0] shiftout;
reg [3:0] color;
reg [2:0] stage;

always_ff @(posedge clk) begin

    if (cs_reg & wr[0] & ~address[0]) hofs[7:0] <= din[7:0];
    if (cs_reg & wr[1] & ~address[0]) hofs[15:8] <= din[15:8];
    if (cs_reg & wr[0] &  address[0]) vofs[7:0] <= din[7:0];
    if (cs_reg & wr[1] &  address[0]) vofs[15:8] <= din[15:8];

    if (ce_pixel) begin
        stage <= stage + 3'd1;

        vcnt_prev <= vcnt;
        if (vcnt_prev != vcnt) begin
            stage <= 3'd0;
        end

        color_out <= { color[3:0], shiftout[31:28]};
        shiftout <= {shiftout[27:0], 4'b0000};

        case(H[2:0])
        3'd0: tileref <= tileref_q;
        3'd7: begin
            color <= tileref[15:12];
            shiftout <= gfx_data;
        end
        endcase
    end
end


endmodule