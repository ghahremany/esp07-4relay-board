#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
KiCad 7 PCB generator — ESP-07 4-Relay Board V2 (ECA-22665 + RF433)
Coordinates: mm, origin = board top-left, from measured manufacturer photos.
Channel scheme (silk): RELAY1=D04=GPIO4, RELAY2=D05=GPIO5, RELAY3=D12=GPIO12, RELAY4=D13=GPIO13.
"""
import uuid, math, os, json

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "hardware", "kicad")
os.makedirs(OUT, exist_ok=True)
W, H = 72.0, 66.0
u = lambda: str(uuid.uuid4())

NETS = ["", "GND", "+12V", "+5V", "+3V3", "USB5V",
        "CH1", "CH2", "CH3", "CH4",            # GPIO drive: CH1=GPIO4, CH2=GPIO5, CH3=GPIO12, CH4=GPIO13
        "K1N", "K2N", "K3N", "K4N",            # coil low side per channel
        "T1D", "T2D", "T3D", "T4D",            # terminal COM
        "T1NO", "T2NO", "T3NO", "T4NO",
        "T1NC", "T2NC", "T3NC", "T4NC",
        "IO2", "IO15", "FLASH0", "ADC", "TXD", "RXD", "RST_N", "EN",
        "SW", "GPIO16", "RF_RXD", "RF_TXD", "TXV5",
        "USB_DP", "USB_DM", "LED1", "LED2", "LED3", "LED4", "PWR"]
NIDX = {n: i for i, n in enumerate(NETS)}
def nid(n):
    if n == "" or n == "NC": return 0
    return NIDX[n]

parts, tracks, vias = [], [], []
def seg(layer, w, net, x1, y1, x2, y2): tracks.append((layer, w, nid(net), (x1, y1), (x2, y2), net))
def via(net, x, y, size=0.8, drill=0.4): vias.append((nid(net), x, y, size, drill))

def fp_open(libfp, layer, x, y, deg=0, attr="smd"):
    s = [f'(footprint "{libfp}" (layer "{layer}.Cu") (tstamp {u()})',
         f'  (at {x:.3f} {y:.3f} {deg})',
         f'  (property "Reference" "REF**" (at 0 -2 0) (layer "{layer}.SilkS") (tstamp {u()}) (effects (font (size 1 1) (thickness 0.15))))',
         f'  (property "Value" "Val**" (at 0 2 0) (layer "{layer}.Fab") (tstamp {u()}) (effects (font (size 1 1) (thickness 0.15)) (justify mirror)))' if layer=="B" else
         f'  (property "Value" "Val**" (at 0 2 0) (layer "{layer}.Fab") (tstamp {u()}) (effects (font (size 1 1) (thickness 0.15))))',
         f'  (attr {attr})']
    return s
def add(s): parts.append("\n".join(s) + "\n)")
def th(s, num, net, x, y, size=2.5, drill=1.3, shape="circle"):
    s.append(f'  (pad "{num}" thru_hole {shape} (at {x:.3f} {y:.3f}) (size {size} {size}) (drill {drill}) (layers "*.Cu" "*.Mask") (remove_unused_layers no) (tstamp {u()}) (net {nid(net)} "{net if nid(net) else ""}"))')
def pd(s, num, net, x, y, w, h, layer="F", shape="rect"):
    s.append(f'  (pad "{num}" smd {shape} (at {x:.3f} {y:.3f}) (size {w} {h}) (layers "{layer}.Cu" "{layer}.Paste" "{layer}.Mask") (tstamp {u()}) (net {nid(net)} "{net if nid(net) else ""}"))')
def sl(s, x1, y1, x2, y2, layer="F", w=0.15):
    s.append(f'  (fp_line (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f}) (stroke (width {w}) (type solid)) (layer "{layer}.SilkS") (tstamp {u()}))')
def st(s, txt, x, y, deg=0, layer="F", size=1.0, w=0.15):
    mir = " (justify mirror)" if layer == "B" else ""
    s.append(f'  (fp_text user "{txt}" (at {x} {y} {deg}) (layer "{layer}.SilkS") (tstamp {u()}) (effects (font (size {size} {size}) (thickness {w})){mir}))')

# ================= RELAYS + TERMINALS =================
RELX = [13.2, 29.4, 45.4, 61.3]; RELY = 26.1     # relay fp anchor (body center at y-8.5)
TBX  = [12.6, 28.7, 44.5, 60.1]; TBY = 3.0
# official SANYOU SRD Form C local pads (rot 270 applied manually):
# 1 COM(0,0) 2 coil-(6.05,-1.95) 3 NO(6.05,-14.15) 4 NC(-6.0,-14.2) 5 coil+(-5.95,-1.95)
for i, cx in enumerate(RELX):
    ch = 4 - i                                    # left→right: RELAY4..RELAY1
    s = fp_open("Relay_THT:Relay_SPDT_SANYOU_SRD_Series_Form_C", "F", cx, RELY, 0, "through_hole")
    for px in (-7.8, 7.8): sl(s, px, -9.9, px, 9.9)
    for py in (-9.9, 9.9): sl(s, -7.8, py, 7.8, py)
    st(s, f"K{ch} RELAY{ch}", 0, -10.8, size=0.9)
    th(s, "1", f"T{ch}D",  0, 0)
    th(s, "2", f"K{ch}N",  6.05, -1.95, 2.5, 1.1)
    th(s, "3", f"T{ch}NO", 6.05, -14.15)
    th(s, "4", f"T{ch}NC", -6.0, -14.2)
    th(s, "5", "+12V",     -5.95, -1.95, 2.5, 1.1)
    add(s)
    tx = TBX[i]
    s = fp_open("TerminalBlock:Terminal_Block_3P_P5.0mm", "F", tx, TBY, 0, "through_hole")
    for px in (-7.5, 7.5): sl(s, px, -4.5, px, 4.5)
    for py in (-4.5, 4.5): sl(s, -7.5, py, 7.5, py)
    st(s, f"TB{ch}", 0, -5.8, size=0.8)
    th(s, "1", f"T{ch}NC", -5, 0, 2.6, 1.4)
    th(s, "2", f"T{ch}D",   0, 0, 2.6, 1.4)
    th(s, "3", f"T{ch}NO",  5, 0, 2.6, 1.4)
    add(s)
    seg("F.Cu", 0.6, f"T{ch}NO", tx + 5, TBY, cx + 6.05, RELY - 14.15)
    seg("F.Cu", 0.6, f"T{ch}D",  tx, TBY, cx, RELY)
    seg("F.Cu", 0.6, f"T{ch}NC", tx - 5, TBY, cx - 6.0, RELY - 14.2)

# ================= ESP-07 MODULE + BREAKOUT =================
MX, MY = 29.4, 44.7
LPAD = ["GND", "IO15", "IO2", "FLASH0", "CH1", "CH2", "RXD", "TXD"]          # pins 9..16 top→bottom
RPAD = ["+3V3", "CH4", "CH3", "RF_RXD", "GPIO16", "EN", "ADC", "RST_N"]      # pins 8..1
s = fp_open("RF_Module:ESP-07", "F", MX, MY, 0)
for px in (-8, 8): sl(s, px, -10.6, px, 10.6)
for py in (-10.6, 10.6): sl(s, -8, py, 8, py)
st(s, "U4 ESP-07", 0, -11.6)
for j, n in enumerate(LPAD): pd(s, str(9 + j), n, -8.45, -7 + j * 2, 1.9, 0.95)
for j, n in enumerate(RPAD): pd(s, str(8 - j), n,  8.45, -7 + j * 2, 1.9, 0.95)
add(s)
for side, nets, x in [(-1, LPAD, 19.4), (1, RPAD, 39.4)]:
    s = fp_open("Connector:SolderPad_1x8", "F", x, MY, 0)
    for j, n in enumerate(nets): pd(s, str(j + 1), n, 0, -7 + j * 2, 1.7, 1.0, shape="oval")
    add(s)
    for j in range(8):
        seg("F.Cu", 0.4, nets[j], MX + side * 8.45, MY - 7 + j * 2, x, MY - 7 + j * 2)

# ================= POWER =================
BX, BY = 52.5, 27.0   # LM2576-5 D2PAK-5 (bottom, as on the real board)
s = fp_open("Package_TO_SOT_SMD:TO-263-5", "B", BX, BY, 0)
st(s, "U1 LM2576-5", 0, -6.6, layer="B")
for k, n in enumerate(["+12V", "GND", "+5V", "GND", "SW"]):
    pd(s, str(k + 1), n, -3.4 + k * 1.7, -4.3, 1.3, 2.2, layer="B")
pd(s, "6", "SW", 0, 4.3, 7.9, 4.0, layer="B")
add(s)

LX2, LY2 = 50.7, 34.3   # drum inductor 100uH
s = fp_open("Inductor_THT:L_Radial_D8_P3.5", "F", LX2, LY2, 0, "through_hole")
st(s, "L1 100uH", 0, -6, size=0.8)
th(s, "1", "SW", -1.75, 0, 2.2, 1.1)
th(s, "2", "+5V", 1.75, 0, 2.2, 1.1)
add(s)

for (cx, cy, pos, ref) in [(58.7, 37.7, "+5V", "C1 1000uF/16V"), (51.4, 50.3, "+12V", "C2 1000uF/16V")]:
    s = fp_open("Capacitor_THT:CP_Radial_D10.0mm_P5.00mm", "F", cx, cy, 0, "through_hole")
    st(s, ref, 0, -6.2, size=0.7)
    th(s, "1", pos, -2.5, 0, 2.2, 1.1, "rect")
    th(s, "2", "GND", 2.5, 0, 2.2, 1.1)
    add(s)

JX, JY = 59.4, 51.0
s = fp_open("Connector_BarrelJack:BarrelJack_Horizontal", "F", JX, JY, 0, "through_hole")
for px in (-6.8, 6.8): sl(s, px, -5, px, 5)
for py in (-5, 5): sl(s, -6.8, py, 6.8, py)
st(s, "J1 12V", 0, -6.4)
th(s, "1", "+12V", 0, 4.0, 3.2, 1.5)
th(s, "2", "GND", -3.7, 4.0, 3.2, 1.5)
th(s, "3", "GND", 3.7, 4.0, 3.2, 1.5)
add(s)

D1X, D1Y = 55.0, 44.0                 # 12V protection diode SS34
s = fp_open("Diode_SMD:D_SMA", "F", D1X, D1Y, 0)
st(s, "D1 SS34", 0, -1.9, size=0.6)
pd(s, "1", "+12V", -1.7, 0, 1.7, 1.4)
pd(s, "2", "+12V", 1.7, 0, 1.7, 1.4)
add(s)
seg("F.Cu", 1.0, "+12V", JX, JY + 4, D1X - 1.7, D1Y)
seg("F.Cu", 1.0, "+12V", D1X + 1.7, D1Y, 68, D1Y)
via("+12V", 68, D1Y)
seg("B.Cu", 1.0, "+12V", 68, D1Y, BX - 3.4, BY)
via("SW", 60, 30)
seg("F.Cu", 1.0, "SW", LX2 - 1.75, LY2, 60, 30)
seg("B.Cu", 1.0, "SW", 60, 30, BX, BY + 4.3)
via("+5V", 47, 40)
seg("F.Cu", 1.0, "+5V", LX2 + 1.75, LY2, 58.7 - 2.5, 37.7, )
seg("F.Cu", 1.0, "+5V", 58.7 - 2.5, 37.7, 47, 40)
seg("B.Cu", 0.8, "+5V", 47, 40, 42.3, 41.0)
seg("F.Cu", 1.0, "+12V", 68, 44.0, 68, 27.0)     # feed to coil bus (via)
via("+12V", 68, 27.0)

A3X, A3Y = 40.0, 41.0                 # AMS1117-3.3 SOT-223 bottom
s = fp_open("Package_TO_SOT_SMD:SOT-223-3_TabPin2", "B", A3X, A3Y, 0)
st(s, "U3 1117-3.3", 0, -3.2, layer="B", size=0.7)
pd(s, "1", "GND", -2.3, 0, 1.05, 2.0, layer="B")
pd(s, "2", "+3V3", 0, 0, 1.05, 2.0, layer="B")
pd(s, "3", "+5V", 2.3, 0, 1.05, 2.0, layer="B")
pd(s, "4", "+3V3", 0, 4.62, 3.05, 2.0, layer="B")
add(s)
seg("B.Cu", 0.6, "+5V", 47, 40, A3X + 2.3, A3Y)
seg("B.Cu", 0.6, "+3V3", A3X, A3Y + 4.62, 37.9, MY - 7)   # tab → module VCC pad(8)
via("+3V3", 37.9, MY - 7)

# coil +12V bus on bottom
bus = [(cx - 5.95, RELY + 1.95) for cx in RELX]     # pad5 global approx (rot: (-5.95,-1.95)→ rel)
copad = [(cx - 5.95, RELY - 1.95) for cx in RELX]
for k in range(3): seg("B.Cu", 0.8, "+12V", *copad[k], *copad[k + 1])
seg("B.Cu", 0.8, "+12V", *copad[0], 8, 24)
seg("B.Cu", 0.8, "+12V", *copad[3], 68, 27)

# ================= CHANNEL DRIVERS (bottom, under each relay) =================
# channel order left→right: CH4, CH3, CH2, CH1 ; GPIO: 13,12,5,4
DMAP = [("CH4", 4), ("CH3", 3), ("CH2", 2), ("CH1", 1)]
base_via = []
for i, cx in enumerate(RELX):
    chn, chnum = DMAP[i]
    s = fp_open("Package_TO_SOT_SMD:SOT-23", "B", cx, 8.6, 0)
    st(s, f"Q{chnum} BC817", 0, -2.5, layer="B", size=0.6)
    pd(s, "1", chn, -0.95, 1.1, 0.9, 1.0, layer="B")
    pd(s, "2", "GND", 0.95, 1.1, 0.9, 1.0, layer="B")
    pd(s, "3", f"K{chnum}N", 0, -1.1, 1.0, 2.0, layer="B")
    add(s)
    via(chn, cx - 2.2, 10.5)
    base_via.append((chn, cx - 2.2, 10.5))
    seg("B.Cu", 0.35, chn, cx - 2.2, 10.5, cx - 0.95, 9.7)
# GPIO → driver: module pad side; CH1(-8.45, MY-1) CH2(MY+1) CH3(8.45, MY-3) CH4(MY-5)
chsrc = {"CH1": (MX - 8.45, MY - 1), "CH2": (MX - 8.45, MY + 1),
         "CH3": (MX + 8.45, MY - 3), "CH4": (MX + 8.45, MY - 5)}
chdst = {"CH4": base_via[0], "CH3": base_via[1], "CH2": base_via[2], "CH1": base_via[3]}
for n, (x1, y1) in chsrc.items():
    _, x2, y2 = chdst[n]
    seg("F.Cu", 0.3, n, x1, y1, x1, 12.0)
    seg("F.Cu", 0.3, n, x1, 12.0, x2, 12.0)
    seg("F.Cu", 0.3, n, x2, 12.0, x2, y2)

# collector stubs to coil pads (top side via)
for i, cx in enumerate(RELX):
    chnum = DMAP[i][1]
    ccx, ccy = cx + 6.05, RELY - 1.95
    via(f"K{chnum}N", cx + 2, 12.6)
    seg("F.Cu", 0.5, f"K{chnum}N", cx + 2, 12.6, ccx, 12.6)
    seg("F.Cu", 0.5, f"K{chnum}N", ccx, 12.6, ccx, ccy)
    seg("B.Cu", 0.5, f"K{chnum}N", cx, 7.5, cx + 2, 12.6)

# ================= LED CHANNEL INDICATORS (top) =================
ledxs = [8.6, 24.7, 40.9, 56.7]
for i, x in enumerate(ledxs):
    chnum = DMAP[i][1]
    s = fp_open("LED_SMD:LED_0805_2012Metric", "F", x, 29.0, 0)
    st(s, f"LD{chnum}", 0, -1.7, size=0.55)
    pd(s, "1", "GND", -0.95, 0, 0.8, 0.95)
    pd(s, "2", f"LED{chnum}", 0.95, 0, 0.8, 0.95)
    add(s)
    s = fp_open("Resistor_SMD:R_0805_2012Metric", "F", x - 3.2, 29.0, 0)
    st(s, f"R{8+chnum} 1k", 0, -1.6, size=0.5)
    pd(s, "1", "+3V3", -0.95, 0, 0.8, 0.95)
    pd(s, "2", f"LED{chnum}", 0.95, 0, 0.8, 0.95)
    add(s)
    seg("F.Cu", 0.35, f"LED{chnum}", x - 2.25, 29.0, x - 0.95, 29.0)

# ================= CH340G (bottom) + USB + CRYSTAL =================
CHX, CHY = 9.0, 46.0
s = fp_open("Package_SO:SSOP-16_3.9x9.9mm_P1.27mm_hack", "B", CHX, CHY, 90, attr="smd")
st(s, "U2 CH340G", 0, -6.0, layer="B", size=0.7)
lp = ["GND", "TXD", "RXD", "USB5V", "GND", "GND", "GND", "USB5V"]
rp = ["USB_DM", "USB_DP", "GND", "GND", "GND", "GND", "GND", "+3V3"]
for j in range(8):
    pd(s, str(1 + j), lp[j], -4.445 + j * 1.27, -2.7, 0.62, 1.55, layer="B")
    pd(s, str(9 + j), rp[7 - j], -4.445 + j * 1.27, 2.7, 0.62, 1.55, layer="B")
add(s)
# reset/flash auto net stub: CH340 DTR->RST via R17/Q5 (documented; placed as SOT-23)
s = fp_open("Package_TO_SOT_SMD:SOT-23", "B", 15, 52, 0)
st(s, "Q5 autoRST", 0, -2.3, layer="B", size=0.55)
pd(s, "1", "RST_N", -0.95, 1.1, 0.9, 1.0, layer="B")
pd(s, "2", "TXD", 0.95, 1.1, 0.9, 1.0, layer="B")
pd(s, "3", "GND", 0, -1.1, 1.0, 2.0, layer="B")
add(s)

s = fp_open("Connector_USB:USB_Micro-B_hack", "F", 2.5, 39.2, 270, attr="smd")
for px in (-4.2, 4.2): sl(s, -3.3, px, 1.8, px)
st(s, "J2 microUSB", 2.5, -5.6, 270, size=0.6)
usp = [("1", "USB5V"), ("2", "USB_DM"), ("3", "USB_DP"), ("4", ""), ("5", "GND")]
for k, (pn, netn) in enumerate(usp):
    x = (2 - k) * 0.65
    s.append(f'  (pad "{pn}" smd rect (at {x:.2f} -1.35) (size 0.42 1.35) (layers "F.Cu" "F.Paste" "F.Mask") (tstamp {u()}) (net {nid(netn)} "{netn}"))')
for (x, y) in [(-3.75, -1.55), (-3.75, 1.55), (3.55, -2.0), (3.55, 2.0)]:
    s.append(f'  (pad "SH" smd rect (at {x} {y}) (size 1.9 1.1) (layers "F.Cu" "F.Mask" "F.Paste") (tstamp {u()}))')
add(s)
# USB data → CH340 (B.Cu, with vias)
via("USB_DM", 5.2, 38.6); via("USB_DP", 5.2, 39.8)
seg("F.Cu", 0.3, "USB_DM", 2.5 + 1.35, 39.2 - 0.65, 5.2, 38.6)
seg("F.Cu", 0.3, "USB_DP", 2.5 + 1.35, 39.2, 5.2, 39.8)
seg("B.Cu", 0.3, "USB_DM", 5.2, 38.6, CHX - 1.175, CHY - 2.7)
seg("B.Cu", 0.3, "USB_DP", 5.2, 39.8, CHX - 3.175, CHY - 2.7)

s = fp_open("Crystal:Crystal_HC49-U_Vertical_hack", "F", 17.7, 40.1, 0, attr="through_hole")
th(s, "1", "", 0, -2.44, 1.8, 1.0)
th(s, "2", "", 0, 2.44, 1.8, 1.0)
st(s, "X1 12MHz", 0, -4.4, size=0.6)
add(s)

# yellow LED + series R
s = fp_open("LED_SMD:LED_0805_2012Metric", "F", 2.2, 46.7, 0)
pd(s, "1", "GND", -0.95, 0, 0.8, 0.95)
pd(s, "2", "PWR", 0.95, 0, 0.8, 0.95)
st(s, "LD PWR", 0, -1.7, size=0.55)
add(s)
s = fp_open("Resistor_SMD:R_0805_2012Metric", "F", 2.2, 50.7, 0)
pd(s, "1", "USB5V", -0.95, 0, 0.8, 0.95)
pd(s, "2", "PWR", 0.95, 0, 0.8, 0.95)
st(s, "R3 1k", 0, -1.6, size=0.5)
add(s)
seg("F.Cu", 0.4, "PWR", 3.15, 46.7, 3.15, 50.7)

# ================= BUTTONS =================
for (bx, by, ref, netn) in [(1.9, 11.9, "SW1 RESET", "RST_N"), (1.9, 21.3, "SW2 FLASH", "FLASH0")]:
    s = fp_open("Button_Switch_SMD:SW_SPST_TL3342_hack", "F", bx, by, 0)
    pd(s, "1", netn, -2.05, -1.05, 1.55, 1.0)
    pd(s, "1", netn, -2.05, 1.05, 1.55, 1.0)
    pd(s, "2", "GND", 2.05, -1.05, 1.55, 1.0)
    pd(s, "2", "GND", 2.05, 1.05, 1.55, 1.0)
    st(s, ref, 5.4, 0, 90, size=0.55)
    add(s)
    seg("F.Cu", 0.35, netn, bx + 2.05, by, MX - 8.45, MY + (7 if netn == "RST_N" else -3))

# ================= mounting holes =================
for (hx, hy) in [(2, 2), (70, 2), (2, 54), (70, 54), (2, 64), (70, 64)]:
    s = [f'(footprint "MountingHole:MountingHole_3.2mm_M3_hack" (layer "F.Cu") (tstamp {u()})',
         f'  (at {hx} {hy})',
         f'  (property "Reference" "MH" (at 0 -3 0) (layer "F.SilkS") (tstamp {u()}) (effects (font (size 0.8 0.8) (thickness 0.15))))',
         f'  (attr exclude_from_pos_files)',
         f'  (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers "*.Cu" "*.Mask") (tstamp {u()}))']
    add(s)

# ================= RF433 SECTION (V2, z=56..66) =================
s = fp_open("RF_Module:RXB6_hack", "F", 11.0, 61.0, 0, attr="through_hole")
for px in (-13, 13): sl(s, px, -6.5, px, 6.5)
for py in (-6.5, 6.5): sl(s, -13, py, 13, py)
st(s, "U5 RXB6/MX-RM-5V", 0, -7.9, size=0.75)
th(s, "1", "+5V", -6, 4.5, 2.0, 1.0)
th(s, "2", "RF_RXD", -4, 4.5, 2.0, 1.0)
th(s, "3", "RF_RXD", -2, 4.5, 2.0, 1.0)
th(s, "4", "GND", 0, 4.5, 2.0, 1.0)
th(s, "5", "", 8, -4.5, 2.0, 1.0)
add(s)

TXMX = 44.0
s = fp_open("RF_Module:FS1000A_hack", "F", TXMX, 61.5, 0, attr="through_hole")
for px in (-9.5, 9.5): sl(s, px, -9.5, px, 9.5)
for py in (-9.5, 9.5): sl(s, -9.5, py, 9.5, py)
st(s, "U6 FS1000A", 0, -11.0, size=0.75)
th(s, "1", "TXV5", -3, 8.0, 2.0, 1.0, "rect")
th(s, "2", "RF_TXD", -1, 8.0, 2.0, 1.0)
th(s, "3", "GND", 1, 8.0, 2.0, 1.0)
th(s, "4", "", 7, -6.0, 2.0, 1.0)
add(s)

def r0805(x, y, n1, n2, ref, layer="F"):
    s = fp_open("Resistor_SMD:R_0805_2012Metric", layer, x, y, 0)
    pd(s, "1", n1, -0.95, 0, 0.8, 0.95, layer=layer)
    pd(s, "2", n2, 0.95, 0, 0.8, 0.95, layer=layer)
    st(s, ref, 0, -1.6, layer=layer, size=0.55)
    add(s)

# RX divider upper resistor (RXB6 DATA --10k--> towards GPIO14)
r0805(44.0, 43.8, "RF_RXD", "RF_RXD", "R21 10k")

# TX driver: GPIO16 → R22 → Q7(2N7002) gate; Q7 drain → TXV5-low-side switch? Keep simple series resistor to data line:
s = fp_open("Resistor_SMD:R_0805_2012Metric", "F", 47.5, 43.8, 0)
pd(s, "1", "GPIO16", -0.95, 0, 0.8, 0.95)
pd(s, "2", "RF_TXD", 0.95, 0, 0.8, 0.95)
st(s, "R22 1k", 0, -1.6, size=0.55)
add(s)
s = fp_open("Package_TO_SOT_SMD:SOT-23", "F", 51.0, 46.5, 0)
st(s, "Q7 2N7002", 0, -2.4, size=0.55)
pd(s, "1", "RF_TXD", -0.95, 1.1, 0.9, 1.0)
pd(s, "2", "GND", 0.95, 1.1, 0.9, 1.0)
pd(s, "3", "", 0, -1.1, 1.0, 2.0)
add(s)

# links: GPIO14 breakout ↔ R20; R20 ↔ RXB6 data; R22/RF_TXD ↔ FS1000A data
seg("F.Cu", 0.3, "RF_RXD", 39.4, MY - 1, 45.55, 43.8)
seg("F.Cu", 0.3, "RF_RXD", 43.05, 43.8, 7.0, 65.5)
seg("F.Cu", 0.3, "GPIO16", 39.4, MY + 1, 46.55, 43.8)
seg("F.Cu", 0.3, "RF_TXD", 48.45, 43.8, TXMX - 1, 53.5)
seg("F.Cu", 0.5, "TXV5", TXMX - 3, 61.5 + 8.0, 58.0, 55.0)
seg("F.Cu", 0.5, "+5V", 58.0, 55.0, 56.1, 37.7)   # RF rail from C1+

# ================= header / misc / zones =================
L = []
L.append('(kicad_pcb (version 20221018) (generator "arena-agent") (generator_version "7.0")')
L.append('  (general (thickness 1.6) (legacy_teardrops no))')
L.append('  (paper "A3")')
L.append('''  (layers
    (0 "F.Cu" signal) (31 "B.Cu" signal)
    (32 "B.Adhes" user "B.Adhesive") (33 "F.Adhes" user "F.Adhesive")
    (34 "B.Paste" user) (35 "F.Paste" user)
    (36 "B.SilkS" user "B.Silkscreen") (37 "F.SilkS" user "F.Silkscreen")
    (38 "B.Mask" user) (39 "F.Mask" user)
    (40 "Dwgs.User" user "User.Drawings") (41 "Cmts.User" user "User.Comments")
    (42 "Eco1.User" user "User.Eco1") (43 "Eco2.User" user "User.Eco2")
    (44 "Edge.Cuts" user) (45 "Margin" user)
    (46 "B.CrtYd" user "B.Courtyard") (47 "F.CrtYd" user "F.Courtyard")
    (48 "B.Fab" user) (49 "F.Fab" user))''')
L.append('''  (setup (pad_to_mask_clearance 0.05) (allow_soldermask_bridges_in_footprints no)
    (pcbplotparams (layerselection 0x00010fc_ffffffff) (plot_on_all_layers_selection 0x0000000_00000000)
      (disableapertmacros no) (usegerberextensions no) (usegerberattributes yes) (usegerberadvancedattributes yes)
      (creategerberjobfile yes) (svguseinch no) (svgprecision 4) (plotframeref no) (viasonmask no) (mode 1)
      (plotreference yes) (plotvalue yes) (plotfptext yes) (plotinvisibletext no) (sketchpadsonfab no)
      (subtractmaskfromsilk no) (outputformat 1) (mirror no) (drillshape 1) (scaleselection 1) (outputdirectory "")))''')
L.append('  (net 0 "")')
for i in range(1, len(NETS)):
    if NETS[i]: L.append(f'  (net {i} "{NETS[i]}")')

M = []
M.append(f'  (gr_rect (start 0.5 0.5) (end {W-0.5:.1f} {H-0.5:.1f}) (stroke (width 0.15) (type solid)) (fill none) (layer "Edge.Cuts") (tstamp {u()}))')
M.append(f'  (gr_text "ESP07 ix4 Relay Board V2 — ECA-22665 + RF433" (at 36 68.3 0) (layer "F.Fab") (tstamp {u()}) (effects (font (size 1.5 1.5) (thickness 0.22))))')
M.append(f'  (gr_text "RL1..4←GPIO4/5/12/13 · RX→GPIO14 · TX←GPIO16" (at 36 70.3 0) (layer "F.Fab") (tstamp {u()}) (effects (font (size 1.1 1.1) (thickness 0.18))))')
for lyr in ("F.Cu", "B.Cu"):
    M.append(f'''  (zone (net 1) (net_name "GND") (layer "{lyr}") (tstamp {u()})
    (hatch edge 0.508) (connect_pads (clearance 0.3))
    (min_thickness 0.25) (filled_areas_thickness no)
    (fill (thermal_gap 0.4) (thermal_bridge_width 0.4))
    (polygon (pts (xy 0.8 0.8) (xy {W-0.8:.1f} 0.8) (xy {W-0.8:.1f} {H-0.8:.1f}) (xy 0.8 {H-0.8:.1f}))))''')
T = []
for (layer, w, n, (x1, y1), (x2, y2), net) in tracks:
    T.append(f'  (segment (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f}) (width {w}) (layer "{layer}") (net {n}) (tstamp {u()}))')
for (n, x, y, size, drill) in vias:
    T.append(f'  (via (at {x:.3f} {y:.3f}) (size {size}) (drill {drill}) (layers "F.Cu" "B.Cu") (net {n}) (tstamp {u()}))')

pcb = "\n".join(L + parts + T + M) + "\n)\n"
open(OUT + "/esp07_4relay.kicad_pcb", "w", encoding="utf-8").write(pcb)
print("pcb:", len(pcb), "bytes |", len(parts), "footprints |", len(tracks), "segments |", len(vias), "vias")

pro = {"board": {"design_settings": {"defaults": {}, "diff_pair_dimensions": [], "drc_exclusions": [],
    "meta": {"version": 2}, "rule_severities": {},
    "rules": {"max_error": 0.005, "min_clearance": 0.2, "min_copper_edge_clearance": 0.3, "min_hole_clearance": 0.25,
      "min_track_width": 0.2, "min_via_annulus": 0.15, "min_via_diameter": 0.6, "solder_mask_clearance": 0.05},
    "track_widths": [0.3, 0.35, 0.5, 0.8, 1.0], "via_dimensions": [], "zones_allow_external_fills": False},
   "ipc2581": {"dist": "", "distpn": "", "internal_id": "", "pn": "", "version": ""}, "layer_presets": []},
  "boards": [], "cvpcb": {"equivalence_files": []},
  "erc": {"erc_exclusions": [], "meta": {"version": 0}, "pin_map": [], "rule_severities": {}},
  "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
  "meta": {"filename": "esp07_4relay.kicad_pro", "version": 1},
  "net_settings": {"classes": [], "meta": {"version": 3}, "net_colors": None, "netclass_assignments": None, "netclass_patterns": []},
  "pcbnew": {"last_paths": {"gencad": "", "idf": "", "netlist": "", "specctra_dsn": "", "step": "", "stl": "", "vrml": ""}, "page_layout_descr_file": ""},
  "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []}, "sheets": [], "text_variables": {}}
open(OUT + "/esp07_4relay.kicad_pro", "w").write(json.dumps(pro, indent=2))
print("pro written")
