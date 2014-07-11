include <pins.scad>;

difference(){
	union(){
		main();
		verticals();
		mount();
	}
	holes();
	rotate([180,0,0])translate([-34,-27.5,-4]){
		translate([14,2.5,0])cylinder(h=5,r=1.7,$fn=30);
		translate([66,7.62,0])cylinder(h=5,r=1.7,$fn=30);
		translate([15.25,50.8,0])cylinder(h=5,r=1.7,$fn=30);
		translate([66.7,35.50,0])cylinder(h=5,r=1.7,$fn=30);
	}
}

module mount(){
	difference(){
		cylinder(h=75, r=25.4, $fn=60);
		translate([0,0,10])cylinder(h=66, r=19.6, $fn=60);
		translate([0,0,60])rotate([0,90,0])cylinder(h=80, r=3.43, $fn=30, center=true);
	}
}

module main(){
	difference(){
		cylinder(h=10,r1=85,r2=90,$fn=60);
		translate([0,0,1.5])cylinder(h=8.6,r1=82,r2=87,$fn=60);
	}
}

module verticals(){
	translate([60,0,0])cylinder(h=10,r=7.5,$fn=30);
	rotate([0,0,120])translate([60,0,0])cylinder(h=10,r=7.5,$fn=30);
	rotate([0,0,-120])translate([60,0,0])cylinder(h=10,r=7.5,$fn=30);
}

module holes(){
	translate([60,0,0])pinhole(h=10);
	rotate([0,0,120])translate([60,0,0])pinhole(h=10);
	rotate([0,0,-120])translate([60,0,0])pinhole(h=10);
}
