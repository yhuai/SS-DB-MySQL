#!/usr/bin/perl -W

#takes no arguments, just presumes you have run "./benchGen -f Raw -t" in this directory

$path = `pwd`;
chomp($path);
#need to already have entered password once to make this work - presumably when starting mysqld_safe
#create db
`mysql -u root  -S/home/jennie/install/mysql/mysql.sock -e \"create database scidb\"`;

#create images table
print "mysql -u root -S/home/jennie/install/mysql/mysql.sock  -D scidb -e \"CREATE TABLE images (imageid INT NOT NULL PRIMARY KEY, xstart INT NOT NULL, ystart INT NOT NULL, xend INT NOT NULL, yend INT NOT NULL, time INT NOT NULL, cycle INT NOT NULL, tablename VARCHAR(20) NOT NULL) ENGINE=MyISAM\"";

print "Creating symlink\n";
`ln -s Raw.pos images.pos`;
print "inserting image tuples\n";
print "mysql -D scidb  -u root -S/home/jennie/install/mysql/mysql.sock -e \" LOAD DATA LOCAL INFILE '/home/jennie/scidb/rdbms_benchmark/scidb_rdb/images.pos' INTO TABLE images FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n'\"\n";

open FILE, "Raw.pos";
@data = <FILE>;
close FILE;

foreach $line (@data)
{
    chomp($line);
    @components = split(/,/, $line);
    $tablename = $components[$#components];
    $abs_path = $path . "/" . $tablename;
    print "Working on $tablename\n";
    print "mysql  -u root -S/home/jennie/install/mysql/mysql.sock -D scidb -e \"create table $tablename(tile INT NOT NULL, x INT NOT NULL, y INT NOT NULL, pix INT NOT NULL, var INT NOT NULL, valid INT NOT NULL, sat INT NOT NULL, v0 INT NOT NULL, v1 INT NOT NULL, v2 INT NOT NULL, v3 INT NOT NULL, v4 INT NOT NULL, v5 INT NOT NULL, v6 INT NOT NULL, CONSTRAINT PRIMARY KEY Raw_x_PK (tile, y, x)) ENGINE=INNODB\"\n";
    print "mysql -u root -S/home/jennie/install/mysql/mysql.sock -D scidb -e \" LOAD DATA LOCAL INFILE '$abs_path' INTO TABLE $tablename FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n'\"\n";
}
