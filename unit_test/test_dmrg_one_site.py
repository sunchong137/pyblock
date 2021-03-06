
from pyblock.qchem import BlockHamiltonian, LineCoupling, DMRGContractor
from pyblock.qchem import MPSInfo, MPOInfo, MPS, MPO
from pyblock.qchem import DMRGDataPage, Simplifier, AllRules
from pyblock.algorithm import DMRG

import numpy as np
import pytest
import fractions
import os

@pytest.fixture
def data_dir(request):
    filename = request.module.__file__
    return os.path.join(os.path.dirname(filename), 'data')

@pytest.fixture(scope="module", params=[1, 2, 3, 4])
def dot_scheme(request):
    return request.param

@pytest.fixture(scope="module", params=[True, False])
def use_su2(request):
    return request.param

class TestDMRGOneSite:
    def test_n2_sto3g_simpl_dot(self, data_dir, tmp_path, dot_scheme, use_su2):
        fcidump = 'N2.STO3G.FCIDUMP'
        pg = 'd2h'
        page = DMRGDataPage(tmp_path / 'node0')
        simpl = Simplifier(AllRules(su2=use_su2))
        with BlockHamiltonian.get(os.path.join(data_dir, fcidump), pg, su2=use_su2, output_level=-1,
                                  memory=2000, page=page) as hamil:
            lcp = LineCoupling(hamil.n_sites, hamil.site_basis, hamil.empty, hamil.target)
            lcp.set_bond_dimension(60)
            mps = MPS(lcp, center=0, dot=1 if dot_scheme == 1 else 2)
            mps.randomize()
            mps.canonicalize()
            mpo = MPO(hamil)
            ctr = DMRGContractor(MPSInfo(lcp), MPOInfo(hamil), simpl)
            tto = dot_scheme if dot_scheme >= 3 else -1
            dmrg = DMRG(mpo, mps, bond_dims=[60, 100],
                        noise=[1E-5, 1E-5, 1E-6, 1E-7, 1E-7, 0], contractor=ctr)
            ener = dmrg.solve(10, 1E-6, two_dot_to_one_dot=tto)
            if use_su2:
                assert abs(ener - (-107.648250974014)) < 5E-5
            else:
                assert abs(ener - (-107.648250974014)) < 5E-4
        page.clean()
    
    def test_n2_sto3g_simpl_exact(self, data_dir, tmp_path):
        fcidump = 'N2.STO3G.FCIDUMP'
        pg = 'd2h'
        page = DMRGDataPage(tmp_path / 'node0')
        simpl = Simplifier(AllRules())
        with BlockHamiltonian.get(os.path.join(data_dir, fcidump), pg, su2=True, output_level=-1,
                                  memory=2000, page=page) as hamil:
            lcp = LineCoupling(hamil.n_sites, hamil.site_basis, hamil.empty, hamil.target)
            lcp.set_bond_dimension(60, exact=True)
            mps = MPS(lcp, center=0, dot=2)
            mps.randomize()
            mps.canonicalize()
            mpo = MPO(hamil)
            ctr = DMRGContractor(MPSInfo(lcp), MPOInfo(hamil), simpl)
            dmrg = DMRG(mpo, mps, bond_dims=[60, 100],
                        noise=[5E-4, 1E-4, 1E-4, 1E-4, 1E-5, 0], contractor=ctr)
            ener = dmrg.solve(10, 1E-6, two_dot_to_one_dot=-1)
            assert abs(ener - (-107.648250974014)) < 5E-6
        page.clean()
    
    def test_n2_sto3g_simpl_occ(self, data_dir, tmp_path, use_su2):
        fcidump = 'N2.STO3G-OCC.FCIDUMP'
        occfile = 'N2.STO3G-OCC.OCC'
        occ = [float(x) for x in open(os.path.join(data_dir, occfile), 'r').read().split()]
        pg = 'd2h'
        page = DMRGDataPage(tmp_path / 'node0')
        simpl = Simplifier(AllRules(su2=use_su2))
        with BlockHamiltonian.get(os.path.join(data_dir, fcidump), pg, su2=use_su2, output_level=-1,
                                  memory=2000, page=page) as hamil:
            lcp = LineCoupling(hamil.n_sites, hamil.site_basis, hamil.empty, hamil.target)
            lcp.set_bond_dimension_using_occ(50, occ=occ, bias=10000)
            mps = MPS(lcp, center=0, dot=2)
            mps.randomize()
            mps.canonicalize()
            mpo = MPO(hamil)
            ctr = DMRGContractor(MPSInfo(lcp), MPOInfo(hamil), simpl)
            dmrg = DMRG(mpo, mps, bond_dims=[50, 100],
                        noise=[1E-3, 1E-4, 1E-4, 1E-4, 1E-5, 0], contractor=ctr)
            ener = dmrg.solve(10, 1E-6, two_dot_to_one_dot=-1)
            assert abs(ener - (-107.65412244752243)) < 1E-4
        page.clean()
