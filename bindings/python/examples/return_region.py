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

from __future__ import print_function

import legion
from legion import task, RW

@task
def make_region():
    # If you return a region from a task, the privileges to the region
    # will be automatically given to the calling task.
    R = legion.Region.create([4, 4], {'x': legion.float64})
    print('returning from make_region with', R)
    return R

@task
def make_region_dict():
    # It should also work if the region in question is returned as
    # part of a larger data structure.
    R = legion.Region.create([4, 4], {'x': legion.float64})
    result = {'asdf': R}
    print('returning from make_region_dict with', result)
    return result

@task(privileges=[RW])
def use_region(R):
    print('in use_region with', R)
    R.x.fill(0)

@task
def main():
    R = make_region().get()
    use_region(R)

    print('in main with', R)
    R.x.fill(1)

    R2 = make_region_dict().get()['asdf']
    use_region(R2)

    print('in main with', R2)
    R2.x.fill(1)

if __name__ == '__legion_main__':
    main()