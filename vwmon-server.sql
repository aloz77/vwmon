
CREATE TABLE IF NOT EXISTS `vwmon_history` (
  `datetime` datetime NOT NULL COMMENT 'UTC',
  `amu_wp_stat` tinyint(4) DEFAULT NULL,
  `amu_comp_h_sum` decimal(6,1) DEFAULT NULL,
  `amu_comp_starts` decimal(6,1) DEFAULT NULL,
  `amu_boiler_h_sum` decimal(6,1) DEFAULT NULL,
  `amu_boiler_starts` decimal(6,1) DEFAULT NULL,
  `mv_yield_sum` decimal(7,1) DEFAULT NULL,
  `mv_brine_in_temp` decimal(6,4) DEFAULT NULL,
  `mv_brine_out_temp` decimal(6,4) DEFAULT NULL,
  `mv_brine_press_press` decimal(5,3) DEFAULT NULL,
  `mv_brine_press_stat` tinyint(4) DEFAULT NULL,
  `mv_high_press_press` decimal(5,3) DEFAULT NULL,
  `mv_low_press_press` decimal(5,3) DEFAULT NULL,
  `mv_overheat_temp` decimal(6,4) DEFAULT NULL,
  `mv_undercool_temp` decimal(6,4) DEFAULT NULL,
  `mv_EI_current` decimal(4,1) DEFAULT NULL,
  `mv_comp_in_temp_temp` decimal(7,4) DEFAULT NULL,
  `mv_comp_out_temp_temp` decimal(7,4) DEFAULT NULL,
  `mv_tev_in_temp_temp` decimal(7,4) DEFAULT NULL,
  `mv_comp_io` tinyint(4) DEFAULT NULL,
  `mv_boiler_io` tinyint(4) DEFAULT NULL,
  `mv_T6_temp_temp` decimal(7,4) DEFAULT NULL,
  `mv_T5_temp_temp` decimal(7,4) DEFAULT NULL,
  `mv_heat_press_press` decimal(5,3) DEFAULT NULL,
  `mv_heat_pump_io` tinyint(4) DEFAULT NULL,
  `mv_VF2_temp_temp` decimal(7,4) DEFAULT NULL,
  `ci_cir2_set_temp` decimal(7,4) DEFAULT NULL,
  `mv_mixer_stat` tinyint(4) DEFAULT NULL,
  `mv_pump_stat` tinyint(4) DEFAULT NULL,
  `mv_boiler_temp_temp` decimal(7,4) DEFAULT NULL,
  `mv_circ_pump_io` tinyint(4) DEFAULT NULL,
  `mv_valve` tinyint(4) DEFAULT NULL,
  `mv_power_interrupt` tinyint(4) DEFAULT NULL,
  `mv_at_temp_temp` decimal(7,4) DEFAULT NULL,
  `hw_mode` tinyint(4) DEFAULT NULL,
  `hw_temp` decimal(4,1) DEFAULT NULL,
  `hw_min_temp` decimal(4,1) DEFAULT NULL,
  `cir2_rt_day` decimal(4,1) DEFAULT NULL,
  `cir2_rt_night` decimal(4,1) DEFAULT NULL,
  `cir2_curve` decimal(4,2) DEFAULT NULL,
  `cir2_at_off` decimal(4,1) DEFAULT NULL,
  UNIQUE KEY `datetime` (`datetime`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='VWMon History';

CREATE TABLE IF NOT EXISTS `vwmon_commands` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `datetime_created` datetime NOT NULL COMMENT 'UTC',
  `command` varchar(255) NOT NULL,
  `status` tinyint(4) NOT NULL COMMENT '0=New,1=Sent,2=Finished',
  `feedback` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM  DEFAULT CHARSET=utf8 AUTO_INCREMENT=30 ;

CREATE TABLE IF NOT EXISTS `vwmon_wp_stat` (
  `id` int(11) DEFAULT '0',
  `description` varchar(255) NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

INSERT INTO `vwmon_wp_stat` (`id`, `description`) VALUES
(0, 'Abgeschaltet'),
(3, 'Heizen'),
(6, 'Pause'),
(7, 'Solevorlauf'),
(8, 'Warmwasser'),
(10, 'Pumpenlauf'),
(12, 'Soleentlüftung'),
(NULL, 'Unbekannt (NULL)'),
(2, 'Unbekannter Status 2'),
(4, 'Unbekannter Status 4'),
(1, 'Unbekannter Status 1'),
(5, 'Unbekannter Status 5'),
(9, 'Unbekannter Status 9'),
(11, 'Unbekannter Status 11');
