#!/usr/bin/env python

# Copyright 2019 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import absolute_import, division, print_function, unicode_literals

from collections import OrderedDict
import numpy as np
import os
import subprocess

import legion
from legion import index_launch, print_once, task, Fspace, Future, IndexLaunch, Ispace, ID, N, Partition, R, Reduce, Region, RW

root_dir = os.path.dirname(__file__)
try:
    prefix_dir = legion.prefix_dir
except AttributeError:
    prefix_dir, legion_h_path = legion.find_legion_header()
pennant_header = subprocess.check_output(
    [
        "gcc", "-I", prefix_dir, "-DLEGION_USE_PYTHON_CFFI", "-DLEGION_MAX_DIM=%s" % legion._max_dim, "-DREALM_MAX_DIM=%s" % legion._max_dim, "-E", "-P",
        os.path.join(root_dir, "pennant_config.h")
    ]).decode("utf-8")
ffi = legion.ffi
ffi.cdef(pennant_header)

mesh_colorings = legion.Type(
    np.dtype([('bytes', np.void, ffi.sizeof('mesh_colorings'))]),
    'mesh_colorings')

mesh_partitions = legion.Type(
    np.dtype([('bytes', np.void, ffi.sizeof('mesh_partitions'))]),
    'mesh_partitions')

config = legion.Type(
    np.dtype([('bytes', np.void, ffi.sizeof('config'))]),
    'config')

def create_partition(is_disjoint, region, c_partition, color_space):
    ipart = legion.Ipartition(c_partition.index_partition, region.ispace, color_space)
    return legion.Partition.create(region, ipart)

_constant_time_launches = False
if _constant_time_launches:
    extern_task = legion.extern_task_wrapper
else:
    extern_task = legion.extern_task

read_config = legion.extern_task(
    task_id=10000,
    argument_types=[],
    privileges=[],
    return_type=config,
    calling_convention='regent')

read_partitions = legion.extern_task(
    task_id=10001,
    argument_types=[Region, Region, Region, config],
    privileges=[N, N, N],
    return_type=mesh_partitions,
    calling_convention='regent')

initialize_topology = extern_task(
    task_id=10003,
    argument_types=[config, legion.int64, Region, Region, Region, Region, Region],
    privileges=[
        None,
        None,
        RW('znump'),
        RW('px_x', 'px_y', 'has_bcx', 'has_bcy'),
        RW('px_x', 'px_y', 'has_bcx', 'has_bcy'),
        N,
        RW('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r', 'mapss3', 'mapss4')],
    return_type=legion.void,
    calling_convention='regent')

init_pointers = extern_task(
    task_id=10004,
    argument_types=[Region, Region, Region, Region],
    privileges=[N, N, N, RW('mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r')],
    return_type=legion.void,
    calling_convention='regent')

init_mesh_zones = extern_task(
    task_id=10005,
    argument_types=[Region],
    privileges=[RW('zx_x', 'zx_y', 'zarea', 'zvol')],
    return_type=legion.void,
    calling_convention='regent')

init_side_fracs = extern_task(
    task_id=10006,
    argument_types=[Region, Region, Region, Region],
    privileges=[
        R('zarea'),
        N,
        N,
        R('mapsz', 'sarea') + RW('smf')],
    return_type=legion.void,
    calling_convention='regent')

init_hydro = extern_task(
    task_id=10007,
    argument_types=[Region, legion.float64, legion.float64, legion.float64, legion.float64, legion.float64, legion.float64, legion.float64, legion.float64],
    privileges=[
        R('zx_x', 'zx_y', 'zvol') + RW('zr', 'ze', 'zwrate', 'zm', 'zetot')],
    return_type=legion.void,
    calling_convention='regent')

init_radial_velocity = extern_task(
    task_id=10008,
    argument_types=[Region, legion.float64],
    privileges=[
        R('px_x', 'px_y') + RW('pu_x', 'pu_y')],
    return_type=legion.void,
    calling_convention='regent')

init_step_points = extern_task(
    task_id=10009,
    argument_types=[Region, legion.bool_],
    privileges=[
        RW('pmaswt', 'pf_x', 'pf_y')],
    return_type=legion.void,
    calling_convention='regent')

adv_pos_half = extern_task(
    task_id=10010,
    argument_types=[Region, legion.float64, legion.bool_],
    privileges=[
        R('px_x', 'px_y', 'pu_x', 'pu_y') + RW('px0_x', 'px0_y', 'pxp_x', 'pxp_y', 'pu0_x', 'pu0_y')],
    return_type=legion.void,
    calling_convention='regent')

init_step_zones = extern_task(
    task_id=10011,
    argument_types=[Region, legion.bool_],
    privileges=[
        R('zvol') + RW('zvol0')],
    return_type=legion.void,
    calling_convention='regent')

calc_centers = extern_task(
    task_id=10012,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('znump') + RW('zxp_x', 'zxp_y'),
        R('pxp_x', 'pxp_y'),
        R('pxp_x', 'pxp_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r') + RW('exp_x', 'exp_y')],
    return_type=legion.void,
    calling_convention='regent')

calc_volumes = extern_task(
    task_id=10013,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('zxp_x', 'zxp_y', 'znump') + RW('zareap', 'zvolp'),
        R('pxp_x', 'pxp_y'),
        R('pxp_x', 'pxp_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r') + RW('sareap', 'elen')],
    return_type=legion.void, # int32,
    calling_convention='regent')

calc_char_len = extern_task(
    task_id=10014,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('znump') + RW('zdl'),
        N,
        N,
        R('mapsz', 'sareap', 'elen')],
    return_type=legion.void,
    calling_convention='regent')

calc_rho_half = extern_task(
    task_id=10015,
    argument_types=[Region, legion.bool_],
    privileges=[
        R('zvolp', 'zm') + RW('zrp')],
    return_type=legion.void,
    calling_convention='regent')

sum_point_mass = extern_task(
    task_id=10016,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('zareap', 'zrp'),
        RW('pmaswt'),
        Reduce('+', 'pmaswt'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapss3', 'smf')],
    return_type=legion.void,
    calling_convention='regent')

calc_state_at_half = extern_task(
    task_id=10017,
    argument_types=[Region, legion.float64, legion.float64, legion.float64, legion.bool_],
    privileges=[
        R('zvol0', 'zvolp', 'zm', 'zr', 'ze', 'zwrate') + RW('zp', 'zss')],
    return_type=legion.void,
    calling_convention='regent')

calc_force_pgas_tts = extern_task(
    task_id=10018,
    argument_types=[Region, Region, Region, Region, legion.float64, legion.float64, legion.bool_],
    privileges=[
        R('zxp_x', 'zxp_y', 'zareap', 'zrp', 'zss', 'zp'),
        N,
        N,
        R('mapsz', 'sareap', 'smf', 'exp_x', 'exp_y') + RW('sfp_x', 'sfp_y', 'sft_x', 'sft_y')],
    return_type=legion.void,
    calling_convention='regent')

qcs_zone_center_velocity = extern_task(
    task_id=10019,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('znump') + RW('zuc_x', 'zuc_y'),
        R('pu_x', 'pu_y'),
        R('pu_x', 'pu_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r')],
    return_type=legion.void,
    calling_convention='regent')

qcs_corner_divergence = extern_task(
    task_id=10020,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('zxp_x', 'zxp_y', 'zuc_x', 'zuc_y'),
        R('pxp_x', 'pxp_y', 'pu_x', 'pu_y'),
        R('pxp_x', 'pxp_y', 'pu_x', 'pu_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r', 'mapss3', 'exp_x', 'exp_y', 'elen') + RW('carea', 'ccos', 'cdiv', 'cevol', 'cdu')],
    return_type=legion.void,
    calling_convention='regent')

qcs_qcn_force = extern_task(
    task_id=10021,
    argument_types=[Region, Region, Region, Region, legion.float64, legion.float64, legion.float64, legion.bool_],
    privileges=[
        R('zrp', 'zss'),
        R('pu_x', 'pu_y'),
        R('pu_x', 'pu_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r', 'mapss3', 'elen', 'cdiv', 'cdu', 'cevol') + RW('cqe1_x', 'cqe1_y', 'cqe2_x', 'cqe2_y')],
    return_type=legion.void,
    calling_convention='regent')

qcs_force = extern_task(
    task_id=10022,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        N,
        N,
        N,
        R('mapss4', 'elen', 'carea', 'ccos', 'cqe1_x', 'cqe1_y', 'cqe2_x', 'cqe2_y') + RW('sfq_x', 'sfq_y')],
    return_type=legion.void,
    calling_convention='regent')

qcs_vel_diff = extern_task(
    task_id=10023,
    argument_types=[Region, Region, Region, Region, legion.float64, legion.float64, legion.bool_],
    privileges=[
        R('zss') + RW('zdu', 'z0tmp'),
        R('pxp_x', 'pxp_y', 'pu_x', 'pu_y'),
        R('pxp_x', 'pxp_y', 'pu_x', 'pu_y'),
        R('mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r', 'mapsz', 'elen')],
    return_type=legion.void,
    calling_convention='regent')

sum_point_force = extern_task(
    task_id=10024,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('znump'),
        RW('pf_x', 'pf_y'),
        Reduce('+', 'pf_x', 'pf_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapss3', 'sfq_x', 'sfq_y', 'sft_x', 'sft_y')],
    return_type=legion.void,
    calling_convention='regent')

apply_boundary_conditions = extern_task(
    task_id=10025,
    argument_types=[Region, legion.bool_],
    privileges=[
        R('has_bcx', 'has_bcy') + RW('pu0_x', 'pu0_y', 'pf_x', 'pf_y')],
    return_type=legion.void,
    calling_convention='regent')

adv_pos_full = extern_task(
    task_id=10026,
    argument_types=[Region, legion.float64, legion.bool_],
    privileges=[
        R('px0_x', 'px0_y', 'pu0_x', 'pu0_y', 'pf_x', 'pf_y', 'pmaswt') + RW('px_x', 'px_y', 'pu_x', 'pu_y')],
    return_type=legion.void,
    calling_convention='regent')

calc_centers_full = extern_task(
    task_id=10027,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('znump') + RW('zx_x', 'zx_y'),
        R('px_x', 'px_y'),
        R('px_x', 'px_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r') + RW('ex_x', 'ex_y')],
    return_type=legion.void,
    calling_convention='regent')

calc_volumes_full = extern_task(
    task_id=10028,
    argument_types=[Region, Region, Region, Region, legion.bool_],
    privileges=[
        R('zx_x', 'zx_y', 'znump') + RW('zarea', 'zvol'),
        R('px_x', 'px_y'),
        R('px_x', 'px_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r') + RW('sarea')],
    return_type=legion.void, # int32,
    calling_convention='regent')

calc_work = extern_task(
    task_id=10029,
    argument_types=[Region, Region, Region, Region, legion.float64, legion.bool_],
    privileges=[
        R('znump') + RW('zw', 'zetot'),
        R('pxp_x', 'pxp_y', 'pu0_x', 'pu0_y', 'pu_x', 'pu_y'),
        R('pxp_x', 'pxp_y', 'pu0_x', 'pu0_y', 'pu_x', 'pu_y'),
        R('mapsz', 'mapsp1', 'mapsp1_r', 'mapsp2', 'mapsp2_r', 'sfp_x', 'sfp_y', 'sfq_x', 'sfq_y')],
    return_type=legion.void,
    calling_convention='regent')

calc_work_rate_energy_rho_full = extern_task(
    task_id=10030,
    argument_types=[Region, legion.float64, legion.bool_],
    privileges=[
        R('zvol0', 'zvol', 'zm', 'zw', 'zp', 'zetot') + RW('zwrate', 'ze', 'zr')],
    return_type=legion.void,
    calling_convention='regent')

calc_dt_hydro = extern_task(
    task_id=10031,
    argument_types=[Region, legion.float64, legion.float64, legion.float64, legion.float64, legion.bool_, legion.bool_],
    privileges=[
        R('zdl', 'zvol0', 'zvol', 'zss', 'zdu')],
    return_type=legion.float64,
    calling_convention='regent')

calc_global_dt = legion.extern_task(
    task_id=10032,
    argument_types=[legion.float64, legion.float64, legion.float64, legion.float64, legion.float64, legion.float64, legion.float64, legion.int64],
    privileges=[],
    return_type=legion.float64,
    calling_convention='regent')

read_input_sequential = legion.extern_task(
    task_id=10041,
    argument_types=[Region, Region, Region, config],
    privileges=[RW, RW, RW],
    return_type=mesh_colorings,
    calling_convention='regent')

validate_output_sequential = legion.extern_task(
    task_id=10042,
    argument_types=[Region, Region, Region, config],
    privileges=[R, R, R],
    return_type=legion.void,
    calling_convention='regent')

@task(leaf=True, return_type=legion.float64)
def min_task(a, b):
    return min(a, b.get())

@task(task_id=2, replicable=True, inner=True)
def main():
    print_once('Running pennant.py')

    conf = read_config().get()

    zone = Fspace.create(OrderedDict([
        ('zxp_x', legion.float64),
        ('zxp_y', legion.float64),
        ('zx_x', legion.float64),
        ('zx_y', legion.float64),
        ('zareap', legion.float64),
        ('zarea', legion.float64),
        ('zvol0', legion.float64),
        ('zvolp', legion.float64),
        ('zvol', legion.float64),
        ('zdl', legion.float64),
        ('zm', legion.float64),
        ('zrp', legion.float64),
        ('zr', legion.float64),
        ('ze', legion.float64),
        ('zetot', legion.float64),
        ('zw', legion.float64),
        ('zwrate', legion.float64),
        ('zp', legion.float64),
        ('zss', legion.float64),
        ('zdu', legion.float64),
        ('zuc_x', legion.float64),
        ('zuc_y', legion.float64),
        ('z0tmp', legion.float64),
        ('znump', legion.uint8),
    ]))

    point = Fspace.create(OrderedDict([
        ('px0_x', legion.float64),
        ('px0_y', legion.float64),
        ('pxp_x', legion.float64),
        ('pxp_y', legion.float64),
        ('px_x', legion.float64),
        ('px_y', legion.float64),
        ('pu0_x', legion.float64),
        ('pu0_y', legion.float64),
        ('pu_x', legion.float64),
        ('pu_y', legion.float64),
        ('pap_x', legion.float64),
        ('pap_y', legion.float64),
        ('pf_x', legion.float64),
        ('pf_y', legion.float64),
        ('pmaswt', legion.float64),
        ('has_bcx', legion.bool_),
        ('has_bcy', legion.bool_),
    ]))

    side = Fspace.create(OrderedDict([
        ('mapsz', legion.int1d),
        ('mapsp1', legion.int1d),
        ('mapsp1_r', legion.uint8),
        ('mapsp2', legion.int1d),
        ('mapsp2_r', legion.uint8),
        ('mapss3', legion.int1d),
        ('mapss4', legion.int1d),
        ('sareap', legion.float64),
        ('sarea', legion.float64),
        ('svolp', legion.float64),
        ('svol', legion.float64),
        ('ssurfp_x', legion.float64),
        ('ssurfp_y', legion.float64),
        ('smf', legion.float64),
        ('sfp_x', legion.float64),
        ('sfp_y', legion.float64),
        ('sft_x', legion.float64),
        ('sft_y', legion.float64),
        ('sfq_x', legion.float64),
        ('sfq_y', legion.float64),
        ('exp_x', legion.float64),
        ('exp_y', legion.float64),
        ('ex_x', legion.float64),
        ('ex_y', legion.float64),
        ('elen', legion.float64),
        ('carea', legion.float64),
        ('cevol', legion.float64),
        ('cdu', legion.float64),
        ('cdiv', legion.float64),
        ('ccos', legion.float64),
        ('cqe1_x', legion.float64),
        ('cqe1_y', legion.float64),
        ('cqe2_x', legion.float64),
        ('cqe2_y', legion.float64),
    ]))

    zones = Region.create([conf.nz], zone)
    points = Region.create([conf.np], point)
    sides = Region.create([conf.ns], side)

    assert conf.seq_init or conf.par_init, 'enable one of sequential or parallel initialization'

    if conf.seq_init:
        colorings = read_input_sequential(zones, points, sides, conf).get()

    assert conf.par_init
    partitions = read_partitions(zones, points, sides, conf).get()

    pieces = Ispace.create([conf.npieces])

    zones_part = create_partition(True, zones, partitions.rz_all_p, pieces)

    points_part = create_partition(True, points, partitions.rp_all_p, [2])
    private = points_part[0]
    ghost = points_part[1]

    private_part = create_partition(True, private, partitions.rp_all_private_p, pieces)
    ghost_part = create_partition(False, ghost, partitions.rp_all_ghost_p, pieces)
    shared_part = create_partition(True, ghost, partitions.rp_all_shared_p, pieces)

    sides_part = create_partition(True, sides, partitions.rs_all_p, pieces)

    if conf.par_init:
        if _constant_time_launches:
            c = Future(conf, value_type=config)
            index_launch(
                pieces,
                initialize_topology,
                c, ID,
                zones_part[ID],
                private_part[ID],
                shared_part[ID],
                ghost_part[ID],
                sides_part[ID])
        else:
            for i in IndexLaunch(pieces):
                initialize_topology(
                    conf, i,
                    zones_part[i],
                    private_part[i],
                    shared_part[i],
                    ghost_part[i],
                    sides_part[i])

    if _constant_time_launches:
        index_launch(
            pieces,
            init_pointers,
            zones_part[ID],
            private_part[ID],
            ghost_part[ID],
            sides_part[ID])

        index_launch(
            pieces,
            init_mesh_zones,
            zones_part[ID])

        index_launch(
            pieces,
            calc_centers_full,
            zones_part[ID],
            private_part[ID],
            ghost_part[ID],
            sides_part[ID],
            True)

        index_launch(
            pieces,
            calc_volumes_full,
            zones_part[ID],
            private_part[ID],
            ghost_part[ID],
            sides_part[ID],
            True)

        index_launch(
            pieces,
            init_side_fracs,
            zones_part[ID],
            private_part[ID],
            ghost_part[ID],
            sides_part[ID])

        index_launch(
            pieces,
            init_hydro,
            zones_part[ID],
            conf.rinit, conf.einit, conf.rinitsub, conf.einitsub,
            conf.subregion[0], conf.subregion[1], conf.subregion[2], conf.subregion[3])

        index_launch(
            pieces,
            init_radial_velocity,
            private_part[ID], conf.uinitradial)

        index_launch(
            pieces,
            init_radial_velocity,
            shared_part[ID], conf.uinitradial)
    else:
        for i in IndexLaunch(pieces):
            init_pointers(
                zones_part[i],
                private_part[i],
                ghost_part[i],
                sides_part[i])

        for i in IndexLaunch(pieces):
            init_mesh_zones(
                zones_part[i])

        for i in IndexLaunch(pieces):
            calc_centers_full(
                zones_part[i],
                private_part[i],
                ghost_part[i],
                sides_part[i],
                True)

        for i in IndexLaunch(pieces):
            calc_volumes_full(
                zones_part[i],
                private_part[i],
                ghost_part[i],
                sides_part[i],
                True)

        for i in IndexLaunch(pieces):
            init_side_fracs(
                zones_part[i],
                private_part[i],
                ghost_part[i],
                sides_part[i])

        for i in IndexLaunch(pieces):
            init_hydro(
                zones_part[i],
                conf.rinit, conf.einit, conf.rinitsub, conf.einitsub,
                conf.subregion[0], conf.subregion[1], conf.subregion[2], conf.subregion[3])

        for i in IndexLaunch(pieces):
            init_radial_velocity(private_part[i], conf.uinitradial)

        for i in IndexLaunch(pieces):
            init_radial_velocity(shared_part[i], conf.uinitradial)

    cycle = 0
    cstop = conf.cstop + 2*conf.prune
    time = 0.0
    dt = Future(conf.dtmax, legion.float64)
    dthydro = conf.dtmax
    while cycle < cstop and time < conf.tstop:
        if cycle == conf.prune:
            legion.execution_fence(block=True)
            start_time = legion.c.legion_get_current_time_in_nanos()

        if _constant_time_launches:
            index_launch(pieces, init_step_points, private_part[ID], True)

            index_launch(pieces, init_step_points, shared_part[ID], True)

            index_launch(pieces, init_step_zones, zones_part[ID], True)

            dt = calc_global_dt(dt, conf.dtfac, conf.dtinit, conf.dtmax, dthydro, time, conf.tstop, cycle)

            index_launch(pieces, adv_pos_half, private_part[ID], dt, True)

            index_launch(pieces, adv_pos_half, shared_part[ID], dt, True)

            index_launch(
                pieces,
                calc_centers,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                calc_volumes,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                calc_char_len,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(pieces, calc_rho_half, zones_part[ID], True)

            index_launch(
                pieces,
                sum_point_mass,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                calc_state_at_half,
                zones_part[ID],
                conf.gamma, conf.ssmin, dt,
                True)

            index_launch(
                pieces,
                calc_force_pgas_tts,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                conf.alfa, conf.ssmin,
                True)

            index_launch(
                pieces,
                qcs_zone_center_velocity,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                qcs_corner_divergence,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                qcs_qcn_force,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                conf.gamma, conf.q1, conf.q2,
                True)

            index_launch(
                pieces,
                qcs_force,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                qcs_vel_diff,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                conf.q1, conf.q2,
                True)

            index_launch(
                pieces,
                sum_point_force,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(pieces, apply_boundary_conditions,private_part[ID], True)

            index_launch(pieces, apply_boundary_conditions, shared_part[ID], True)

            index_launch(pieces, adv_pos_full, private_part[ID], dt, True)

            index_launch(pieces, adv_pos_full, shared_part[ID], dt, True)

            index_launch(
                pieces,
                calc_centers_full,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                calc_volumes_full,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                True)

            index_launch(
                pieces,
                calc_work,
                zones_part[ID],
                private_part[ID],
                ghost_part[ID],
                sides_part[ID],
                dt,
                True)

            index_launch(
                pieces,
                calc_work_rate_energy_rho_full,
                zones_part[ID],
                dt,
                True)

            future = index_launch(
                pieces,
                calc_dt_hydro,
                zones_part[ID],
                dt, conf.dtmax, conf.cfl, conf.cflv, True,
                reduce='min')

            dthydro = conf.dtmax
            dthydro = min_task(dthydro, future)
        else:
            for i in IndexLaunch(pieces):
                init_step_points(private_part[i], True)

            for i in IndexLaunch(pieces):
                init_step_points(shared_part[i], True)

            for i in IndexLaunch(pieces):
                init_step_zones(zones_part[i], True)

            dt = calc_global_dt(dt, conf.dtfac, conf.dtinit, conf.dtmax, dthydro, time, conf.tstop, cycle)

            for i in IndexLaunch(pieces):
                adv_pos_half(private_part[i], dt, True)

            for i in IndexLaunch(pieces):
                adv_pos_half(shared_part[i], dt, True)

            for i in IndexLaunch(pieces):
                calc_centers(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                calc_volumes(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                calc_char_len(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                calc_rho_half(zones_part[i], True)

            for i in IndexLaunch(pieces):
                sum_point_mass(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                calc_state_at_half(
                    zones_part[i],
                    conf.gamma, conf.ssmin, dt,
                    True)

            for i in IndexLaunch(pieces):
                calc_force_pgas_tts(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    conf.alfa, conf.ssmin,
                    True)

            for i in IndexLaunch(pieces):
                qcs_zone_center_velocity(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                qcs_corner_divergence(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                qcs_qcn_force(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    conf.gamma, conf.q1, conf.q2,
                    True)

            for i in IndexLaunch(pieces):
                qcs_force(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                qcs_vel_diff(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    conf.q1, conf.q2,
                    True)

            for i in IndexLaunch(pieces):
                sum_point_force(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                apply_boundary_conditions(private_part[i], True)

            for i in IndexLaunch(pieces):
                apply_boundary_conditions(shared_part[i], True)

            for i in IndexLaunch(pieces):
                adv_pos_full(private_part[i], dt, True)

            for i in IndexLaunch(pieces):
                adv_pos_full(shared_part[i], dt, True)

            for i in IndexLaunch(pieces):
                calc_centers_full(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                calc_volumes_full(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    True)

            for i in IndexLaunch(pieces):
                calc_work(
                    zones_part[i],
                    private_part[i],
                    ghost_part[i],
                    sides_part[i],
                    dt,
                    True)

            for i in IndexLaunch(pieces):
                calc_work_rate_energy_rho_full(
                    zones_part[i],
                    dt,
                    True)

            futures = []
            for i in IndexLaunch(pieces):
                futures.append(
                    calc_dt_hydro(
                        zones_part[i],
                        dt, conf.dtmax, conf.cfl, conf.cflv, True))

            dthydro = conf.dtmax
            dthydro = min(dthydro, *list(map(lambda x: x.get(), futures)))

        cycle += 1
        time += dt.get()

        if cycle == cstop - conf.prune:
            legion.execution_fence(block=True)
            stop_time = legion.c.legion_get_current_time_in_nanos()

    if conf.seq_init:
        validate_output_sequential(zones, points, sides, conf)
    else:
        print_once("Warning: Skipping sequential validation")

    print_once("ELAPSED TIME = %7.3f s" % ((stop_time - start_time)/1e9))

if __name__ == '__legion_main__':
    main()