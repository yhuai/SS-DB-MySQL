CREATE TABLE images (
  imageid INT NOT NULL PRIMARY KEY,
  xstart INT NOT NULL,
  ystart INT NOT NULL,
  xend INT NOT NULL,
  yend INT NOT NULL,
  time INT NOT NULL,
  cycle INT NOT NULL,
  tablename VARCHAR(20) NOT NULL -- ex. 'Raw_0001', 'Raw_0002'
);

CREATE TABLE observations (
  obsid INT NOT NULL PRIMARY KEY,
  imageid INT NOT NULL REFERENCES images,
  time INT NOT NULL, -- denormalized data for convenience
  cycle INT NOT NULL, -- denormalized data for convenience
-- x/y of center of polygons that define this star
  centerx INT NOT NULL,
  centery INT NOT NULL,
-- bounding box of all polygons of this star
  boxxstart INT NOT NULL,
  boxystart INT NOT NULL,
  boxxend INT NOT NULL,
  boxyend INT NOT NULL,
  center GEOMETRY NOT NULL, bbox GEOMETRY NOT NULL
) ENGINE=MyISAM;

CREATE TABLE obs_polygons (
  obsid INT NOT NULL REFERENCES observations,
  ord INT NOT NULL,
  x INT NOT NULL,
  y INT NOT NULL,
  CONSTRAINT PRIMARY KEY obs_polygons_pk (obsid, ord)
) ENGINE=InnoDB;

CREATE TABLE obsgroup (
  obsgroupid INT NOT NULL PRIMARY KEY,
  first_obsid INT NOT NULL REFERENCES observation,
  centroidx INT NOT NULL,
  centroidy INT NOT NULL,
  boxxstart INT NOT NULL, -- bbox of all centroids of this group's observations
  boxystart INT NOT NULL,
  boxxend INT NOT NULL,
  boxyend INT NOT NULL,
  center GEOMETRY NOT NULL, bbox GEOMETRY NOT NULL,
  center_traj GEOMETRY NOT NULL -- trajectory defined by centroids in Q8. for Q9, use polygon_trajectory
) ENGINE=MyISAM;

CREATE TABLE obsgroup_obs (
  obsgroupid INT NOT NULL REFERENCES obsgroup,
  obsid INT NOT NULL REFERENCES observation,
  time INT NOT NULL, -- denormalized data for convenience
  CONSTRAINT PRIMARY KEY obsgroup_obs_pk (obsgroupid, obsid)
) ENGINE=InnoDB;

CREATE TABLE polygon_trajectory (
  obsgroupid INT NOT NULL REFERENCES obsgroup,
  time INT NOT NULL,
-- This star is observed at cur_obsid at time time, 
-- previously observed at pre_obsid, 
-- next observed at post_obsid. 
-- We assume the star can be at any point in the triangle at time time.
  pre_obsid INT NULL REFERENCES observation, -- NULL if first obs
  cur_obsid INT NOT NULL REFERENCES observation,
  post_obsid INT NULL REFERENCES observation, -- NULL if last obs
-- bbox of all polygons of pre/cur/post obs
  boxxstart INT NOT NULL,
  boxystart INT NOT NULL,
  boxxend INT NOT NULL,
  boxyend INT NOT NULL,
  bbox GEOMETRY NOT NULL,
  CONSTRAINT PRIMARY KEY polygon_traj_pk (obsgroupid, time)
) ENGINE=MyISAM;


