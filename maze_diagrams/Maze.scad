wood_colour = "BlanchedAlmond";
perspex_colour = "LightCyan";

floor_width = 1000; //mm
floor_length = 1000; //mm
wall_height = 80; //mm

floor_thickness = 15; //mm
wall_thickness = 5; //mm

corridor_width = 200; //mm

ramp_angle = 30; //degrees
ramp_coord = [4, 2.5]; //coordinate
floor_separation = 100; //mm

post_thickness = 50; //mm
post_height = (floor_thickness + wall_height) * 2 + floor_separation;

model_separation = 500; //mm

//------------------------------------------------

ramp_height = wall_height + floor_separation + floor_thickness;
ramp_len_hyp = ramp_height / sin(ramp_angle);
ramp_len_base = ramp_height / tan(ramp_angle);

//------------------------------------------------

main();

//------------------------------------------------

module main() {
	//Corridors maze
	corridors();

	//Posts
	posts();

	//Obstacles
	translate([0,0,floor_thickness+wall_height+floor_separation]) {
		obstacles();
	}

	//Separate levels
	translate([
					-(floor_width + (wall_thickness * 2) + model_separation) / 2,
					floor_length + (wall_thickness * 2) + (model_separation * 1.5),
					0
			  ]) {
		corridors();
		translate([
						floor_width + (wall_thickness * 2) + model_separation,
						0,
						0
			 		]) {
			obstacles();
		}
	}
}

//------------------------------------------------

module corridors() {
	//Floor
	color(wood_colour) {
		cube([
				floor_width + (wall_thickness * 2),
				floor_length + (wall_thickness * 2),
				floor_thickness
			 ]);
	}

	//Outer walls
	color(perspex_colour) {
		translate([0, 0, floor_thickness]) {
			cube([wall_thickness, floor_length + (wall_thickness * 2), wall_height]);
			cube([floor_width + (wall_thickness * 2), wall_thickness, wall_height]);
			translate([floor_width + wall_thickness, 0, 0]) {
				cube([wall_thickness, floor_length + (wall_thickness * 2), wall_height]);
			}
			translate([0, floor_length + wall_thickness, 0]) {
				cube([floor_width + (wall_thickness * 2), wall_thickness, wall_height]);
			}
		}	
	}	

	//Inner walls
	color(perspex_colour) {
		for(i = [
					[ 0,1, 2,1 ],
					[ 3,1, 3,2 ],
					[ 3,1, 5,1 ],
					[ 1,2, 3,2 ],
					[ 2,2, 2,4 ],
					[ 2,4, 3,4 ],
					[ 1,3, 1,5 ],
					[ 4,2, 4,5 ],
					[ 3,3, 4,3 ]
				  ])
		{
			assign(sX=i[0], sY=i[1], eX=i[2], eY=i[3]) {
				//Move into position
				translate([
								sX*corridor_width + wall_thickness,
								sY*corridor_width + wall_thickness,
								floor_thickness
						  ]) {
					if (sX != eX) {
						//Horizontal	
						cube([
							(eX-sX)*corridor_width,
							wall_thickness,
							wall_height
							  ]);
					} else {
						//Vertical
					cube([
								wall_thickness,
								(eY-sY)*corridor_width,
								wall_height
							  ]);
					}
				}
			}
		}
	}
	
	//Ramp
	color(perspex_colour) {
		translate([
						ramp_coord[0]*corridor_width + wall_thickness,
						ramp_coord[1]*corridor_width + wall_thickness,
						floor_thickness
					]) {
			rotate(a = ramp_angle, v = [1,0,0])
				cube([corridor_width, ramp_len_hyp, wall_thickness]);
		}
	}
}

//------------------------------------------------

module obstacles() {
	//Floor and ramp entry
	color(wood_colour) {
		difference() {
			cube([
					floor_width + (wall_thickness * 2),
					floor_length + (wall_thickness * 2),
					floor_thickness
				 ]);
			translate([
						ramp_coord[0]*corridor_width + wall_thickness,
						ramp_coord[1]*corridor_width + wall_thickness,
						-5
					 ]) {
				cube([
						corridor_width,
						ramp_len_base,
						floor_thickness + 10
					 ]);
			}
		}
	}

	//Outer walls
	color(perspex_colour) {
		translate([0, 0, floor_thickness]) {
			cube([wall_thickness, floor_length + (wall_thickness * 2), wall_height]);
			cube([floor_width + (wall_thickness * 2), wall_thickness, wall_height]);
			translate([floor_width + wall_thickness, 0, 0]) {
				cube([wall_thickness, floor_length + (wall_thickness * 2), wall_height]);
			}
			translate([0, floor_length + wall_thickness, 0]) {
				cube([floor_width + (wall_thickness * 2), wall_thickness, wall_height]);
			}
		}	
	}

	//Ramp guards
	color(perspex_colour) {
		translate([
					ramp_coord[0]*corridor_width + wall_thickness,
					ramp_coord[1]*corridor_width + wall_thickness,
					floor_thickness
				]) {
			cube([wall_thickness, ramp_len_base, wall_height]);
			cube([corridor_width, wall_thickness, wall_height]);
		}
	}

	//Obstacles
	color(wood_colour) {
		for(i = [
					[ 2,2, 20 ],
					[ 3,2, 10 ],
					[ 0.5,0.5, 20 ],
					[ 1.25,4, 10 ],
					[ 3.5,3.5, 15 ],
					[ 4.5,1.25, 10 ],
					[ 2.75,0.5, 10 ],
					[ 0.5,3, 10 ],
					[ 2.5,4.75, 10 ]
				  ])
		{
			assign(x=i[0], y=i[1], rad=i[2]) {
				//Move into position
				translate([
								x*corridor_width,
								y*corridor_width,
								floor_thickness
							 ]) {
					cylinder(r = rad, h = wall_height);
				}
			}
		}
	}
}

//------------------------------------------------

module posts() {
	//Generate one in each corner
	color(wood_colour) {
		for(x = [0,1]) {
			for(y = [0,1]) {

				//Move into position
				translate([
								-post_thickness/2 + (x * (floor_width + (wall_thickness * 2))),
								-post_thickness/2 + (y * (floor_length + (wall_thickness * 2))),
								0
							 ]) {
					difference() {
						//Post
						cube([
								post_thickness,
								post_thickness,
								post_height
							  ]);
						//Cutout
						translate([
										(post_thickness/2) - (x*(post_thickness+10)),
										(post_thickness/2) - (y*(post_thickness+10)),
										-5
								  ]) {
							cube([
									post_thickness+10,
									post_thickness+10,
									post_height+10
						 		 ]);
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------