$fn = 100; // Number of circle fragments

color_robot = [226 / 255, 227 / 255, 166 / 255];
color_mainboard = [162 / 255, 186 / 255, 166 / 255];
color_fpga = [255 / 255, 160 / 255, 122 / 255];
color_supports = [221 / 255, 160 / 255, 221 / 255];
color_daughter = [166 / 255, 195 / 255, 214 / 255];

c_rad = 47.5; // Chassis radius (mm)
c_thk = 3; // Chassis thickness (mm)

trans_count = 24; // Transducer count
trans_angle = (360 / trans_count); // Transducer spacing angle (deg)
trans_ghost_start = 0; // Transducer ghost start index
trans_ghost_end = 20; // Transducer ghost end index

ww_width = 34; // Wheel well width (mm)
ww_depth = 10.5; // Wheel well depth (mm)

w_rad = 16; // Wheel radius (mm)
w_depth = 6; // Wheel depth (mm)
w_offset = 1; // Wheel offset from wheel well (mm)

cast_rad = 6; // Caster radius (mm)
cast_off = 5; // Caster offset (mm)

mg_width = 15; // Motor and gearbox width (mm)
mg_depth = 25; // Motor and gearbox depth (mm)
mg_height = 10; // Motor and gearbox height (mm)

mb_rad = 47.5; // Mainboard radius (mm)
mb_thk = 3; // Mainboard thickness (mm)
mb_woffeset = 1; // Mainboard wheel top offset (mm)
mb_sup_rad = 1.5; // Mainboard support post radius (mm)
mb_sup_off_x = 25; //Mainboard support post offset X (mm)
mb_sup_off_y = 30; //Mainboard support post offset Y (mm)
mb_con_width = 7; // Mainboard connector width (mm)
mb_con_length = 8; // Mainboard connector length (mm)
mb_con_thk = 4; // Mainboard connector thickness (mm)

fpga_thk = 3; // FPGA thickness (mm)
fpga_width = 35; // FPGA width (mm)
fpga_len_rec = 58; // FPGA rectange section length (mm)
fpga_len_tri = 26; // FPGA triangle section length (mm)
fpga_tri_cut = 15; // FPGA triangle section cutoff length (mm)
fpga_usb_width = 12; // FPGA usb connector width (mm)
fpga_usb_length = 15; // FPGA usb connector length (mm)
fpga_usb_thk = 4; // FPGA usb connector thickness (mm)
fpga_eth_width = 15; // FPGA ethernet width (mm)
fpga_eth_length = 20; // FPGA ethernet length (mm)
fpga_eth_thk = 13; // FPGA ethernet thickness (mm)
fpga_eth_off_x = (fpga_eth_width / 2) - (fpga_width / 2) + 3; // FPGA ethernet offset X (mm)
fpga_eth_off_y = (fpga_eth_length / 2) - (fpga_len_rec / 2) - 4; // FPGA ethernet offset Y (mm)
fpga_con_width = 5; // FPGA connector width (mm)
fpga_con_length = 15; // FPGA connector length (mm)
fpga_con_thk = 10; // FPGA connector thickness (mm)
fpga_conn_sep = 7; // FPGA connector separation (mm)
fpga_conn_off_x = (fpga_con_width / 2) + (fpga_width / 2) - fpga_con_width - 3; // FPGA connector offset X (mm)
fpga_conn_off_y = (fpga_con_length / 2) + (fpga_len_rec / 2) - (fpga_con_length * 2 + fpga_conn_sep); // FPGA connector offset Y (mm)

fpga_mb_offset = 6; // FPGA Mainboard offset (mm)

db_thk = 1; // Daughter board thickness (mm)
db_width = 12; // Daughter board width (mm)
db_length = 40; // Daughter board length (mm)
db_con_width = 7; // Daughter board connector width (mm)
db_con_length = 4; // Daughter board connector width (mm)
db_con_thk = 8; // Daughter board connector thickness (mm)
db_tran_rad = 4.5; // Daughter board transducer radius (mm)
db_tran_thk = 6; // Daughter board transducer thickness (mm)
db_tran_leg_rad = 0.25; // Daughter board transducer leg radius (mm)
db_tran_leg_thk = 4; // Daughter board transducer leg thickness (mm)
db_tran_leg_off = 2.5; // Daughter board transducer leg offset (mm)
db_tran_sep = 15; // Daughter board transducer separation (mm)

//-------------------------------------------------------------------------------

// Robot
color(color_robot)
	robot();

// Mainboard
translate([0, 0, c_thk + (mg_height / 2) + w_rad + mb_woffeset])
	color(color_mainboard)
		mainboard();

// Support posts
mb_sup_height = ((mg_height / 2) + w_rad + mb_woffeset);
for(i = [-1, 1]) {
	for(j = [-1, 1]) {
		translate([i * mb_sup_off_x, j * mb_sup_off_y, c_thk + (mb_sup_height / 2)])
			color(color_supports)
				cylinder(r = mb_sup_rad, h = mb_sup_height, center = true);
	}
}

// FPGA
fpga_tot_height = fpga_thk + fpga_eth_thk;
translate([0, 0, fpga_tot_height + c_thk + mb_sup_height + mb_thk + fpga_mb_offset])
	rotate(v = [0, 1, 0], a = 180)
		color(color_fpga)
			fpga();

// Daughter boards
for(i = [0 : trans_count - 1]) {
	rotate(a = [0, 0, -i * trans_angle])
		translate([0, -c_rad, (db_con_length / 2) + c_thk + mb_sup_height + mb_thk])
			rotate(a = [90, 0, 0])
				if(i > trans_ghost_start && i < trans_ghost_end) {
					color(color_daughter)
						daughter();
				} else {
					%daughter();
				}
}

//-------------------------------------------------------------------------------

module robot() {
	// Chassis and wheel wells
	translate([0, 0, c_thk / 2])
		difference() {
			cylinder(r = c_rad, h = c_thk, center = true);
			for(i = [-1, 1]) {
				translate([i * (c_rad - (ww_depth / 2)), 0, 0])
					cube([ww_depth, ww_width, 2 * c_thk], center = true);
			}
		}

	// Caster
	translate([0, c_rad - cast_rad - cast_off, -cast_rad + (c_thk / 2)])
		sphere(r = cast_rad, center = true);

	// Wheels
	for(i = [-1, 1]) {
		translate([i * (c_rad + (w_depth / 2) - ww_depth + w_offset), 0, c_thk + (mg_height / 2)])
			rotate(v = [0, 1, 0], a = 90)
				cylinder(r = w_rad, h = w_depth, center = true);
	}

	// Motors and gearboxes
	for(i = [-1, 1]) {
		translate([i * (c_rad - ww_depth - (mg_depth / 2)), 0, (mg_height / 2) + c_thk])
			cube([mg_depth, mg_width, mg_height], center = true);
	}
}

module mainboard() {
	// PCB
	translate([0, 0, mb_thk / 2])
		cylinder(r = mb_rad, h = mb_thk, center = true);

	// Connectors
	for(i = [0 : trans_count - 1]) {
		rotate(v = [0, 0, 1], a = i * trans_angle)
			translate([0, -mb_rad + (mb_con_length / 2), (mb_con_thk / 2) + mb_thk])
				cube([mb_con_width, mb_con_length, mb_con_thk], center = true);
	}
}

module fpga() {
	fpga_tot_len = fpga_len_rec + fpga_tri_cut + fpga_usb_length;
	translate([0, (fpga_len_rec / 2) - (fpga_tot_len / 2), fpga_thk / 2]) {
		// PCB
		cube([fpga_width, fpga_len_rec, fpga_thk], center = true);
		translate([0, fpga_len_rec / 2, 0])
			difference() {
				linear_extrude(height = fpga_thk, center = true)
					polygon([[-fpga_width / 2, 0], [0, fpga_len_tri], [fpga_width / 2, 0]]);
				translate([0, fpga_tri_cut + (fpga_len_tri / 2), 0])
					cube([fpga_width * 2, fpga_len_tri, fpga_thk * 2], center = true);
			}

		// USB connector
		translate([0, (fpga_usb_length / 2) + (fpga_len_rec / 2) + fpga_tri_cut, 0])
			cube([fpga_usb_width, fpga_usb_length, fpga_usb_thk], center = true);

		// Ethernet connector
		translate([fpga_eth_off_x, fpga_eth_off_y, (fpga_eth_thk / 2) + (fpga_thk / 2)])
			cube([fpga_eth_width, fpga_eth_length, fpga_eth_thk], center = true);

		// PMOD connectors
		translate([fpga_conn_off_x, fpga_conn_off_y, 0])
			for(i = [0, 1]) {
				translate([0, i * (fpga_conn_sep + fpga_con_length), (fpga_con_thk / 2) + (fpga_thk / 2)])
					cube([fpga_con_width, fpga_con_length, fpga_con_thk], center = true);
			}
	}
}

module daughter() {
	// Connector
	translate([0, 0, (db_con_thk / 2)])
		cube([db_con_width, db_con_length, db_con_thk], center = true);

	// PCB
	translate([0, 0, (db_thk / 2) + db_con_thk])
		cube([db_width, db_length, db_thk], center = true);

	// Transducers
	translate([0, 0, db_con_thk + db_thk])
		for(i = [-1, 1]) {
			translate([0, i * db_tran_sep, 0]) {
				for(j = [-1, 1])
					translate([j * db_tran_leg_off, 0, db_tran_leg_thk / 2])
						cylinder(r = db_tran_leg_rad, h = db_tran_leg_thk, center = true);
				translate([0, 0, (db_tran_thk / 2) + db_tran_leg_thk])
					cylinder(r = db_tran_rad, h = db_tran_thk, center = true);
			}
		}
}