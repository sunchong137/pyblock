
#include "rev/operator_functions.hpp"
#include "SpinQuantum.h"
#include "MatrixBLAS.h"
#include "couplingCoeffs.h"
#include "global.h"
#include "newmat.h"
#include <cmath>
#include <vector>
#include <iostream>
#ifdef _OPENMP
#include <omp.h>
#endif

#define TINY 1.e-20

using namespace std;
using namespace SpinAdapted;

namespace block2 {

// if trace_right == true:
// contract state_info[0] with a; trace state_info[1]; output state_info[2]
// if trace_right == false:
// trace state_info[0]; contract state_info[1] with a; output state_info[2]
// if forward : state_info :: 0 =  left site, 1 = physical, 2 = currect site
// if backward: state_info :: 0 = physical, 1 = right site, 2 = currect site
//
// actual output will be in repr of direct product of state_info[0] and state_info[1]
// but in direct sum spin space, but before truncation after collection
// since it is already collected, the quantum numbers are already sorted in c repr
// so the rotation matrix is the map from collected quantum numbers to a subset of it
// with the same order
// further rotation is required to obtain repr in state_info[2]
// assuming state_info[2] is collected
void TensorTraceElement(const StackSparseMatrix &a, StackSparseMatrix &c,
                        const vector<boost::shared_ptr<StateInfo>> &state_info,
                        StackMatrix &cel, int cq, int cqprime, bool trace_right,
                        double scale) {
    
    if (fabs(scale) < TINY)
        return;

    int aq, aqprime, bq, bqprime, bstates;
    
    assert(state_info.size() == 1);
    const StateInfo *cs = state_info[0].get();
    const StateInfo *ls = cs->leftStateInfo;
    const StateInfo *rs = cs->rightStateInfo;
    
    const char conjC = trace_right ? 'n' : 't';
    const std::vector<int> oldToNewI = cs->oldToNewState.at(cq);
    const std::vector<int> oldToNewJ = cs->oldToNewState.at(cqprime);
    
    int rowstride = 0, colstride = 0;

    for (int oldi = 0; oldi < oldToNewI.size(); oldi++) {
        colstride = 0;
        for (int oldj = 0; oldj < oldToNewJ.size(); oldj++) {
            if (conjC == 'n') {
                aq = cs->leftUnMapQuanta[oldToNewI[oldi]];
                aqprime = cs->leftUnMapQuanta[oldToNewJ[oldj]];
                bq = cs->rightUnMapQuanta[oldToNewI[oldi]];
                bqprime = cs->rightUnMapQuanta[oldToNewJ[oldj]];
                bstates = rs->getquantastates(bq); // bq == bqprime, which is traced (right: 1)
            } else {
                aq = cs->rightUnMapQuanta[oldToNewI[oldi]];
                aqprime = cs->rightUnMapQuanta[oldToNewJ[oldj]];
                bq = cs->leftUnMapQuanta[oldToNewI[oldi]];
                bqprime = cs->leftUnMapQuanta[oldToNewJ[oldj]];
                bstates = ls->getquantastates(bq); // bq == bqprime, which is traced (left: 0)
            }

            if (a.allowed(aq, aqprime) && (bq == bqprime)) {
                
                DiagonalMatrix unitMatrix(bstates);
                unitMatrix = 1.;

                Matrix unity(bstates, bstates);
                unity = unitMatrix;

                if (conjC == 'n') {
                    double scaleb = dmrginp.get_ninej()(
                        ls->quanta[aqprime].get_s().getirrep(),
                        rs->quanta[bqprime].get_s().getirrep(),
                        cs->quanta[cqprime].get_s().getirrep(),
                        a.get_spin().getirrep(), 0, c.get_spin().getirrep(),
                        ls->quanta[aq].get_s().getirrep(),
                        rs->quanta[bq].get_s().getirrep(),
                        cs->quanta[cq].get_s().getirrep());

                    scaleb *= Symmetry::spatial_ninej(
                        ls->quanta[aqprime].get_symm().getirrep(),
                        rs->quanta[bqprime].get_symm().getirrep(),
                        cs->quanta[cqprime].get_symm().getirrep(),
                        a.get_symm().getirrep(), 0, c.get_symm().getirrep(),
                        ls->quanta[aq].get_symm().getirrep(),
                        rs->quanta[bq].get_symm().getirrep(),
                        cs->quanta[cq].get_symm().getirrep());
                    
                    scaleb *= a.get_scaling(ls->quanta[aq], ls->quanta[aqprime]);
                    
                    // no fermion check for trace right A x I(bigger site index)

                    MatrixTensorProduct(a.operator_element(aq, aqprime),
                                        a.conjugacy(), scale, unity, 'n',
                                        scaleb, cel, rowstride, colstride);
                } else {
                    double scaleb = dmrginp.get_ninej()(
                        ls->quanta[bqprime].get_s().getirrep(),
                        rs->quanta[aqprime].get_s().getirrep(),
                        cs->quanta[cqprime].get_s().getirrep(), 0,
                        a.get_spin().getirrep(), c.get_spin().getirrep(),
                        ls->quanta[bq].get_s().getirrep(),
                        rs->quanta[aq].get_s().getirrep(),
                        cs->quanta[cq].get_s().getirrep());
                    scaleb *= Symmetry::spatial_ninej(
                        ls->quanta[bqprime].get_symm().getirrep(),
                        rs->quanta[aqprime].get_symm().getirrep(),
                        cs->quanta[cqprime].get_symm().getirrep(),
                        0, a.get_symm().getirrep(), c.get_symm().getirrep(),
                        ls->quanta[bq].get_symm().getirrep(),
                        rs->quanta[aq].get_symm().getirrep(),
                        cs->quanta[cq].get_symm().getirrep());
                    
                    // fermion check for trace left I(smaller site index) x A
                    
                    scaleb *= a.get_scaling(rs->quanta[aq], rs->quanta[aqprime]);
                    
                    
                    if (a.get_fermion() &&
                        
                        // defined in SpinQuantum
                        IsFermion(ls->quanta[bqprime]))
                        scaleb *= -1.;

                    MatrixTensorProduct(
                        unity, 'n', scaleb, a.operator_element(aq, aqprime),
                        a.conjugacy(), scale, cel, rowstride, colstride);
                    
                }
            }
            colstride += cs->unCollectedStateInfo->quantaStates[oldToNewJ[oldj]];
        }
        rowstride += cs->unCollectedStateInfo->quantaStates[oldToNewI[oldi]];
    }
}

void TensorTrace(const StackSparseMatrix &a, StackSparseMatrix &c,
                 const vector<boost::shared_ptr<StateInfo>> &state_info,
                 bool trace_right, double scale) {
    
    if (fabs(scale) < TINY)
        return;
    
    assert(a.get_initialised() && c.get_initialised());

    std::vector<std::pair<std::pair<int, int>, StackMatrix>> &nonZeroBlocks =
        c.get_nonZeroBlocks();

    int quanta_thrds = dmrginp.quanta_thrds();
#pragma omp parallel for schedule(dynamic) num_threads(quanta_thrds)
    for (int index = 0; index < nonZeroBlocks.size(); index++) {
        int cq = nonZeroBlocks[index].first.first,
            cqprime = nonZeroBlocks[index].first.second;
        TensorTraceElement(a, c, state_info,
            nonZeroBlocks[index].second, cq, cqprime, trace_right, scale);
    }
    
}
    
void TensorTraceDiagonal(const StackSparseMatrix &a, DiagonalMatrix &c,
                         const vector<boost::shared_ptr<StateInfo>> &state_info,
                         bool trace_right, double scale) {
    
    if (fabs(scale) < TINY)
        return;
    
    assert(a.get_initialised());
    
    const StateInfo *cs = state_info[0].get();
    const StateInfo *ls = state_info[0]->leftStateInfo, *rs = state_info[0]->rightStateInfo;

    for (int aq = 0; aq < ls->quanta.size(); ++aq)
        if (!trace_right || a.allowed(aq, aq))
            for (int bq = 0; bq < rs->quanta.size(); ++bq)
                if ((trace_right || a.allowed(bq, bq)) && cs->allowedQuanta(aq, bq)) {
                    // traget state must be s=0, therefore only the first cq
                    int cq = cs->quantaMap(aq, bq)[0];
                    
                    if (trace_right) {
                        
                        DiagonalMatrix unitMatrix(rs->quantaStates[bq]);
                        unitMatrix = 1.;
                        
                        double scaleb = dmrginp.get_ninej()(
                            ls->quanta[aq].get_s().getirrep(),
                            rs->quanta[bq].get_s().getirrep(),
                            cs->quanta[cq].get_s().getirrep(),
                            a.get_spin().getirrep(), 0, 0,
                            ls->quanta[aq].get_s().getirrep(),
                            rs->quanta[bq].get_s().getirrep(),
                            cs->quanta[cq].get_s().getirrep());

                        scaleb *= Symmetry::spatial_ninej(
                            ls->quanta[aq].get_symm().getirrep(),
                            rs->quanta[bq].get_symm().getirrep(),
                            cs->quanta[cq].get_symm().getirrep(),
                            a.get_symm().getirrep(), 0, 0,
                            ls->quanta[aq].get_symm().getirrep(),
                            rs->quanta[bq].get_symm().getirrep(),
                            cs->quanta[cq].get_symm().getirrep());
                        
                        scaleb *= a.get_scaling(ls->quanta[aq], ls->quanta[aq]);

                        // no fermion check for trace right A x I(bigger site index)
                        
                        for (int aq_state = 0; aq_state < ls->quantaStates[aq]; aq_state++)
                            VectorScale(a.operator_element(aq, aq)(aq_state + 1, aq_state + 1)
                                * scale * scaleb, unitMatrix,
                                c.Store() + cs->unBlockedIndex[cq] + aq_state * rs->quantaStates[bq]);

                    } else {
                        double scaleb = dmrginp.get_ninej()(
                            ls->quanta[aq].get_s().getirrep(),
                            rs->quanta[bq].get_s().getirrep(),
                            cs->quanta[cq].get_s().getirrep(), 0,
                            a.get_spin().getirrep(), 0,
                            ls->quanta[aq].get_s().getirrep(),
                            rs->quanta[bq].get_s().getirrep(),
                            cs->quanta[cq].get_s().getirrep());
                        scaleb *= Symmetry::spatial_ninej(
                            ls->quanta[aq].get_symm().getirrep(),
                            rs->quanta[bq].get_symm().getirrep(),
                            cs->quanta[cq].get_symm().getirrep(),
                            0, a.get_symm().getirrep(), 0,
                            ls->quanta[aq].get_symm().getirrep(),
                            rs->quanta[bq].get_symm().getirrep(),
                            cs->quanta[cq].get_symm().getirrep());

                        scaleb *= a.get_scaling(rs->quanta[bq], rs->quanta[bq]);
                        
                        // fermion check for trace left I(smaller site index) x A
                        if (a.get_fermion() && IsFermion(ls->quanta[aq]))
                            scaleb *= -1.0;
                            
                        for (int aq_state = 0; aq_state < ls->quantaStates[aq]; aq_state++)
                            MatrixDiagonalScale(1.0 * scale * scaleb, a.operator_element(bq, bq),
                                c.Store() + cs->unBlockedIndex[cq] + aq_state * rs->quantaStates[bq]);
                        
                    }
                }

}

void TensorProductElement(const StackSparseMatrix &a, const StackSparseMatrix &b, const StackSparseMatrix &c,
                          const vector<boost::shared_ptr<StateInfo>> &state_info,
                          StackMatrix &cel, int cq, int cqprime, double scale) {
    
    if (fabs(scale) < TINY)
        return;

    const StateInfo *brastateinfo, *ketstateinfo;
    const StateInfo *lbraS, *rbraS, *lketS, *rketS;
    
    if (state_info.size() == 1) {
        // same bra/ket case
        brastateinfo = ketstateinfo = state_info[0].get();
        lbraS = lketS = ketstateinfo->leftStateInfo;
        rbraS = rketS = ketstateinfo->rightStateInfo;
    } else {
        // different bra/ket case
        brastateinfo = state_info[0].get();
        lbraS = brastateinfo->leftStateInfo;
        rbraS = brastateinfo->rightStateInfo;
        ketstateinfo = state_info[1].get();
        lketS = ketstateinfo->leftStateInfo;
        rketS = ketstateinfo->rightStateInfo;
    }

    const std::vector<int> &oldToNewI = brastateinfo->oldToNewState.at(cq);
    const std::vector<int> &oldToNewJ = ketstateinfo->oldToNewState.at(cqprime);

    const char conjC = 'n';

    int rowstride = 0, colstride = 0;

    int aq, aqprime, bq, bqprime;

    for (int oldi = 0; oldi < oldToNewI.size(); oldi++) {
        colstride = 0;
        for (int oldj = 0; oldj < oldToNewJ.size(); oldj++) {
            aq = brastateinfo->leftUnMapQuanta[oldToNewI[oldi]];
            aqprime = ketstateinfo->leftUnMapQuanta[oldToNewJ[oldj]];
            bq = brastateinfo->rightUnMapQuanta[oldToNewI[oldi]];
            bqprime = ketstateinfo->rightUnMapQuanta[oldToNewJ[oldj]];

            double scaleA = scale;
            double scaleB = 1.0;
            
            if (a.allowed(aq, aqprime) && b.allowed(bq, bqprime)) {
                scaleB = dmrginp.get_ninej()(
                    lketS->quanta[aqprime].get_s().getirrep(),
                    rketS->quanta[bqprime].get_s().getirrep(),
                    ketstateinfo->quanta[cqprime].get_s().getirrep(),
                    a.get_spin().getirrep(), b.get_spin().getirrep(),
                    c.get_spin().getirrep(),
                    lbraS->quanta[aq].get_s().getirrep(),
                    rbraS->quanta[bq].get_s().getirrep(),
                    brastateinfo->quanta[cq].get_s().getirrep());
                scaleB *= Symmetry::spatial_ninej(
                    lketS->quanta[aqprime].get_symm().getirrep(),
                    rketS->quanta[bqprime].get_symm().getirrep(),
                    ketstateinfo->quanta[cqprime].get_symm().getirrep(),
                    a.get_symm().getirrep(), b.get_symm().getirrep(),
                    c.get_symm().getirrep(),
                    lbraS->quanta[aq].get_symm().getirrep(),
                    rbraS->quanta[bq].get_symm().getirrep(),
                    brastateinfo->quanta[cq].get_symm().getirrep());
                scaleB *= b.get_scaling(rbraS->quanta[bq],
                                        rketS->quanta[bqprime]);
                scaleA *= a.get_scaling(lbraS->quanta[aq],
                                        lketS->quanta[aqprime]);
                if (b.get_fermion() &&
                    IsFermion(lketS->quanta[aqprime]))
                    scaleB *= -1;
                
                MatrixTensorProduct(
                    a.operator_element(aq, aqprime), a.conjugacy(),
                    scaleA, b.operator_element(bq, bqprime),
                    b.conjugacy(), scaleB, cel, rowstride, colstride);
            }
            colstride += ketstateinfo->unCollectedStateInfo->quantaStates[oldToNewJ[oldj]];
        }
        rowstride += brastateinfo->unCollectedStateInfo->quantaStates[oldToNewI[oldi]];
    }
}

void TensorProduct(const StackSparseMatrix &a, const StackSparseMatrix &b, StackSparseMatrix &c,
                   const vector<boost::shared_ptr<StateInfo>> &state_info, double scale) {
    
    if (fabs(scale) < TINY)
        return;
    
    assert(a.get_initialised() && b.get_initialised() && c.get_initialised());

    std::vector<std::pair<std::pair<int, int>, StackMatrix>> &nonZeroBlocks =
        c.get_nonZeroBlocks();

    int quanta_thrds = dmrginp.quanta_thrds();
#pragma omp parallel for schedule(dynamic) num_threads(quanta_thrds)
    for (int index = 0; index < nonZeroBlocks.size(); index++) {
        int cq = nonZeroBlocks[index].first.first,
            cqprime = nonZeroBlocks[index].first.second;
        TensorProductElement(a, b, c, state_info, 
            nonZeroBlocks[index].second, cq, cqprime, scale);
    }

}
    

void Product(const StackSparseMatrix &a, const StackSparseMatrix &b, const StackSparseMatrix &c,
             const StateInfo &state_info, double scale) {
    
    if (fabs(scale) < TINY)
        return;
    
    int rows = c.nrows();
    for (int cq = 0; cq < rows; ++cq)
        for (int cqprime = 0; cqprime < rows; ++cqprime)
            if (c.allowed(cq, cqprime))
                for (int aprime = 0; aprime < rows; aprime++)
                    if (a.allowed(cq, aprime) && b.allowed(aprime, cqprime)) {
                        int apj = state_info.quanta[aprime].get_s().getirrep(),
                            cqj = state_info.quanta[cq].get_s().getirrep(),
                            cqpj = state_info.quanta[cqprime].get_s().getirrep();
                        
                        double factor = a.get_scaling(state_info.quanta[cq],
                                                      state_info.quanta[aprime]);
                        factor *= b.get_scaling(state_info.quanta[aprime],
                                                state_info.quanta[cqprime]);
                        
                        if (dmrginp.spinAdapted()) {

                            factor *=
                                racah(cqpj, b.get_spin().getirrep(), cqj,
                                      a.get_spin().getirrep(), apj,
                                      c.get_spin().getirrep()) *
                                pow((1.0 * c.get_spin().getirrep() + 1.0) *
                                        (1.0 * apj + 1.0),
                                    0.5) *
                                pow(-1.0,
                                    static_cast<int>((b.get_spin().getirrep() +
                                                      a.get_spin().getirrep() -
                                                      c.get_spin().getirrep()) /
                                                     2.0));
                        }
                        
                        MatrixMultiply(
                            a.operator_element(cq, aprime), a.conjugacy(),
                            b.operator_element(aprime, cqprime), b.conjugacy(),
                            c.operator_element(cq, cqprime), scale * factor,
                            1.0);
                    }
}

void TensorProductDiagonal(const StackSparseMatrix &a, const StackSparseMatrix &b, DiagonalMatrix &c,
                           const vector<boost::shared_ptr<StateInfo>> &state_info, double scale) {
    
    if (fabs(scale) < TINY)
        return;
    
    assert(a.get_initialised() && b.get_initialised());
    
    assert(state_info.size() == 1);
    const StateInfo *cs = state_info[0].get();
    const StateInfo *ls = cs->leftStateInfo, *rs = cs->rightStateInfo;

    for (int aq = 0; aq < ls->quanta.size(); ++aq)
        if (a.allowed(aq, aq))
            for (int bq = 0; bq < rs->quanta.size(); ++bq)
                if (b.allowed(bq, bq))
                    if (cs->allowedQuanta(aq, bq)) {
                        // traget state must be s=0, therefore only the first cq
                        int cq = cs->quantaMap(aq, bq)[0];
                        Real scaleA = scale;
                        Real scaleB = 1;
                        
                        scaleB *= dmrginp.get_ninej()(
                            ls->quanta[aq].get_s().getirrep(),
                            rs->quanta[bq].get_s().getirrep(),
                            cs->quanta[cq].get_s().getirrep(),
                            a.get_spin().getirrep(),
                            b.get_spin().getirrep(), 0,
                            ls->quanta[aq].get_s().getirrep(),
                            rs->quanta[bq].get_s().getirrep(),
                            cs->quanta[cq].get_s().getirrep());
                        scaleB *= Symmetry::spatial_ninej(
                            ls->quanta[aq].get_symm().getirrep(),
                            rs->quanta[bq].get_symm().getirrep(),
                            cs->quanta[cq].get_symm().getirrep(),
                            a.get_symm().getirrep(),
                            b.get_symm().getirrep(), 0,
                            ls->quanta[aq].get_symm().getirrep(),
                            rs->quanta[bq].get_symm().getirrep(),
                            cs->quanta[cq].get_symm().getirrep());
                        
                        scaleB *= b.get_scaling(rs->quanta[bq], rs->quanta[bq]);
                        
                        scaleB *= a.get_scaling(ls->quanta[aq], ls->quanta[aq]);

                        if (b.get_fermion() && IsFermion(ls->quanta[aq]))
                            scaleB *= -1.0;
                        // unBlockedIndex actually measures the shift caused by bq_state
                        // for example, unBlockedIndex[0] = 0 unBlockedIndex[1] = quantaStates[bq[cq=1]]
                        for (int aq_state = 0; aq_state < ls->quantaStates[aq]; aq_state++)
                            MatrixDiagonalScale(a.operator_element(aq, aq)(aq_state + 1, aq_state + 1)
                                * scaleA * scaleB, b.operator_element(bq, bq),
                                c.Store() + cs->unBlockedIndex[cq] + aq_state * rs->quantaStates[bq]);
                    }

}
    
void TensorRotate(const StackSparseMatrix &a, StackSparseMatrix &c,
                  const vector<boost::shared_ptr<StateInfo>> &state_info,
                  const vector<boost::shared_ptr<vector<Matrix>>> &rotate_matrices, double scale) {
    
    const StateInfo *old_bras, *old_kets, *new_bras, *new_kets;
    const vector<Matrix> *rotate_bra, *rotate_ket;
    if (state_info.size() == 2) {
        old_bras = old_kets = state_info[0].get();
        new_bras = new_kets = state_info[1].get();
        rotate_bra = rotate_ket = rotate_matrices[0].get();
    } else {
        old_bras = state_info[0].get();
        new_bras = state_info[1].get();
        old_kets = state_info[2].get();
        new_kets = state_info[3].get();
        rotate_bra = rotate_matrices[0].get();
        rotate_ket = rotate_matrices[1].get();
    }
    
    assert(a.get_initialised() && c.get_initialised());

    std::vector<std::pair<std::pair<int, int>, StackMatrix>> &nonZeroBlocks =
        c.get_nonZeroBlocks();
    
    vector<int> new_to_old_map_bra, new_to_old_map_ket;
    for (int old_q = 0; old_q < rotate_bra->size(); ++old_q)
        if ((*rotate_bra)[old_q].Ncols() != 0)
            new_to_old_map_bra.push_back(old_q);
    
    for (int old_q = 0; old_q < rotate_ket->size(); ++old_q)
        if ((*rotate_ket)[old_q].Ncols() != 0)
            new_to_old_map_ket.push_back(old_q);
    
    assert(new_bras->quanta.size() == new_to_old_map_bra.size());
    assert(new_kets->quanta.size() == new_to_old_map_ket.size());
    
    int quanta_thrds = dmrginp.quanta_thrds();
#pragma omp parallel for schedule(dynamic) num_threads(quanta_thrds)
    for (int index = 0; index < nonZeroBlocks.size(); index++) {
        int cq = nonZeroBlocks[index].first.first,
            cqprime = nonZeroBlocks[index].first.second;
        int q = new_to_old_map_bra[cq],
            qprime = new_to_old_map_ket[cqprime];
        
        double factor = scale * a.get_scaling(old_bras->quanta[q], old_kets->quanta[qprime]);
        
        MatrixRotate((*rotate_bra)[q], a.operator_element(q, qprime),
            (*rotate_ket)[qprime], nonZeroBlocks[index].second, a.conjugacy(), factor);
        
    }
    
}
    
// MPO (a x b) act on MPS (c) => MPS (v)
// v = (lq) a(left) .(lq') c .(rq') b(right) (rq)
// ket state info = c state info in lq' x rq'
// bra state info = v staet info in lq x rq
void TensorProductMultiply(const StackSparseMatrix &a, const StackSparseMatrix &b,
                    const StackWavefunction &c, StackWavefunction &v,
                    const vector<boost::shared_ptr<StateInfo>> &state_info, const SpinQuantum op_q, double scale) {
    
    const StateInfo *brastateinfo, *ketstateinfo;
    
    if (state_info.size() == 1) {
        brastateinfo = ketstateinfo = state_info[0].get();
    } else {
        brastateinfo = state_info[0].get();
        ketstateinfo = state_info[1].get();
    }
    
    const int leftBraOpSz = brastateinfo->leftStateInfo->quanta.size();
    const int leftKetOpSz = ketstateinfo->leftStateInfo->quanta.size();
    const int rightBraOpSz = brastateinfo->rightStateInfo->quanta.size();
    const int rightKetOpSz = ketstateinfo->rightStateInfo->quanta.size();

    const StateInfo *lbraS = brastateinfo->leftStateInfo,
                    *rbraS = brastateinfo->rightStateInfo;
    const StateInfo *lketS = ketstateinfo->leftStateInfo,
                    *rketS = ketstateinfo->rightStateInfo;

    const StackSparseMatrix &leftOp = a;
    const StackSparseMatrix &rightOp = b;
    const char leftConj = a.conjugacy();
    const char rightConj = b.conjugacy();
    const std::vector<std::pair<std::pair<int, int>, StackMatrix>>
        &nonZeroBlocks = v.get_nonZeroBlocks();
    
    if (state_info.size() == 2) {
        assert(c.ncols() == rightOp.ncols() && v.ncols() == rightOp.nrows());
        assert(c.nrows() == leftOp.ncols() && v.nrows() == leftOp.nrows());
        assert(lbraS->quanta.size() == leftOp.nrows() && lketS->quanta.size() == leftOp.ncols());
        assert(rbraS->quanta.size() == rightOp.nrows() && rketS->quanta.size() == rightOp.ncols());
    }

    long long maxlen = 0;
    for (int lQ = 0; lQ < leftBraOpSz; lQ++)
        for (int rQPrime = 0; rQPrime < rightKetOpSz; rQPrime++)
            if (maxlen <
                (long long) lbraS->getquantastates(lQ) * rketS->getquantastates(rQPrime))
                maxlen = (long long) lbraS->getquantastates(lQ) *
                         rketS->getquantastates(rQPrime);
    
    int quanta_thrds = dmrginp.quanta_thrds();

    double *dataArray[quanta_thrds];
    for (int q = 0; q < quanta_thrds; q++) {
        dataArray[q] = block2::current_page->allocate(maxlen);
    }

#pragma omp parallel for schedule(dynamic) num_threads(quanta_thrds)
    for (int index = 0; index < nonZeroBlocks.size(); index++) {
        int lQ = nonZeroBlocks[index].first.first,
            rQ = nonZeroBlocks[index].first.second;

        const std::vector<int> &colinds = rightOp.getActiveCols(rQ);
        for (int rrop = 0; rrop < colinds.size(); rrop++) {
            int rQPrime = colinds[rrop];

            const std::vector<int> &rowinds = c.getActiveRows(rQPrime);
            for (int l = 0; l < rowinds.size(); l++) {
                int lQPrime = rowinds[l];
                if (leftOp.allowed(lQ, lQPrime)) {
    
                    StackMatrix m(dataArray[omprank],
                                  lketS->getquantastates(lQPrime),
                                  rbraS->getquantastates(rQ));

                    double factor =
                        scale * leftOp.get_scaling(lbraS->quanta[lQ],
                                                   lketS->quanta[lQPrime]);
                    factor *= dmrginp.get_ninej()(
                        lketS->quanta[lQPrime].get_s().getirrep(),
                        rketS->quanta[rQPrime].get_s().getirrep(),
                        c.get_deltaQuantum(0).get_s().getirrep(),
                        leftOp.get_spin().getirrep(),
                        rightOp.get_spin().getirrep(), op_q.get_s().getirrep(),
                        lbraS->quanta[lQ].get_s().getirrep(),
                        rbraS->quanta[rQ].get_s().getirrep(),
                        v.get_deltaQuantum(0).get_s().getirrep());
                    factor *= Symmetry::spatial_ninej(
                        lketS->quanta[lQPrime].get_symm().getirrep(),
                        rketS->quanta[rQPrime].get_symm().getirrep(),
                        c.get_symm().getirrep(), leftOp.get_symm().getirrep(),
                        rightOp.get_symm().getirrep(),
                        op_q.get_symm().getirrep(),
                        lbraS->quanta[lQ].get_symm().getirrep(),
                        rbraS->quanta[rQ].get_symm().getirrep(),
                        v.get_symm().getirrep());
                    int parity = rightOp.get_fermion() &&
                                         IsFermion(lketS->quanta[lQPrime])
                                     ? -1
                                     : 1;
                    factor *= rightOp.get_scaling(rbraS->quanta[rQ],
                                                  rketS->quanta[rQPrime]);
                    
                    MatrixMultiply(c.operator_element(lQPrime, rQPrime), 'n',
                                   rightOp.operator_element(rQ, rQPrime),
                                   TransposeOf(rightOp.conjugacy()), m, 1.0,
                                   0.);
                    MatrixMultiply(leftOp.operator()(lQ, lQPrime), leftConj, m,
                                   'n', v.operator_element(lQ, rQ),
                                   factor * parity);
                }
            }
        }
    }

    for (int q = quanta_thrds - 1; q > -1; q--) {
        block2::current_page->deallocate(dataArray[q], maxlen);
    }
    
}

// MPO (a x I | I x a) act on MPS (c) => MPS (v)
void TensorTraceMultiply(const StackSparseMatrix &a, const StackWavefunction &c,
                         StackWavefunction &v, const StateInfo &state_info,
                         bool trace_right, double scale) {
    
    const StateInfo *brastateinfo = &state_info;
    const StateInfo *ketstateinfo = &state_info;
    
    const StateInfo *lbraS = brastateinfo->leftStateInfo,
                    *lketS = ketstateinfo->leftStateInfo;
    const StateInfo *rbraS = brastateinfo->rightStateInfo,
                    *rketS = ketstateinfo->rightStateInfo;
    const int leftBraOpSz = brastateinfo->leftStateInfo->quanta.size();
    const int leftKetOpSz = ketstateinfo->leftStateInfo->quanta.size();
    const int rightBraOpSz = brastateinfo->rightStateInfo->quanta.size();
    const int rightKetOpSz = ketstateinfo->rightStateInfo->quanta.size();

    if (trace_right) {
        for (int lQ = 0; lQ < leftBraOpSz; ++lQ) {
            for (int lQPrime = 0; lQPrime < leftKetOpSz; ++lQPrime) {
                if (a.allowed(lQ, lQPrime)) {
                    const StackMatrix &aop = a.operator_element(lQ, lQPrime);
                    for (int rQ = 0; rQ < rightKetOpSz; ++rQ)
                        if (c.allowed(lQPrime, rQ) && v.allowed(lQ, rQ)) {
                            double fac = scale;
                            fac *= dmrginp.get_ninej()(
                                lketS->quanta[lQPrime].get_s().getirrep(),
                                rketS->quanta[rQ].get_s().getirrep(),
                                c.get_deltaQuantum(0).get_s().getirrep(),
                                a.get_spin().getirrep(), 0,
                                a.get_spin().getirrep(),
                                lbraS->quanta[lQ].get_s().getirrep(),
                                rketS->quanta[rQ].get_s().getirrep(),
                                v.get_deltaQuantum(0).get_s().getirrep());
                            fac *= Symmetry::spatial_ninej(
                                lketS->quanta[lQPrime].get_symm().getirrep(),
                                rketS->quanta[rQ].get_symm().getirrep(),
                                c.get_symm().getirrep(),
                                a.get_symm().getirrep(), 0,
                                a.get_symm().getirrep(),
                                lbraS->quanta[lQ].get_symm().getirrep(),
                                rketS->quanta[rQ].get_symm().getirrep(),
                                v.get_symm().getirrep());
                            fac *= a.get_scaling(lbraS->quanta[lQ],
                                                 lketS->quanta[lQPrime]);
                            MatrixMultiply(aop, a.conjugacy(),
                                           c.operator_element(lQPrime, rQ),
                                           c.conjugacy(),
                                           v.operator_element(lQ, rQ), fac);
                        }
                }
            }
        }
    } else {
        for (int rQ = 0; rQ < rightBraOpSz; ++rQ) {
            for (int rQPrime = 0; rQPrime < rightKetOpSz; ++rQPrime)
                if (a.allowed(rQ, rQPrime)) {
                    const StackMatrix &aop = a.operator_element(rQ, rQPrime);
                    for (int lQPrime = 0; lQPrime < leftKetOpSz; ++lQPrime)
                        if (v.allowed(lQPrime, rQ) &&
                            c.allowed(lQPrime, rQPrime)) {
                            double fac = scale;
                            fac *= dmrginp.get_ninej()(
                                lketS->quanta[lQPrime].get_s().getirrep(),
                                rketS->quanta[rQPrime].get_s().getirrep(),
                                c.get_deltaQuantum(0).get_s().getirrep(), 0,
                                a.get_spin().getirrep(),
                                a.get_spin().getirrep(),
                                lketS->quanta[lQPrime].get_s().getirrep(),
                                rbraS->quanta[rQ].get_s().getirrep(),
                                v.get_deltaQuantum(0).get_s().getirrep());
                            fac *= Symmetry::spatial_ninej(
                                lketS->quanta[lQPrime].get_symm().getirrep(),
                                rketS->quanta[rQPrime].get_symm().getirrep(),
                                c.get_symm().getirrep(), 0,
                                a.get_symm().getirrep(),
                                a.get_symm().getirrep(),
                                lketS->quanta[lQPrime].get_symm().getirrep(),
                                rbraS->quanta[rQ].get_symm().getirrep(),
                                v.get_symm().getirrep());
                            fac *= a.get_scaling(rbraS->quanta[rQ],
                                                 rketS->quanta[rQPrime]);
                            double parity =
                                a.get_fermion() &&
                                        IsFermion(lketS->quanta[lQPrime])
                                    ? -1
                                    : 1;

                            MatrixMultiply(
                                c.operator_element(lQPrime, rQPrime),
                                c.conjugacy(), aop, TransposeOf(a.conjugacy()),
                                v.operator_element(lQPrime, rQ), fac * parity);
                        }
                }
        }
    }
}
    
void TensorScale(double scale, StackSparseMatrix &a) {
    
    assert(a.get_initialised());

    std::vector<std::pair<std::pair<int, int>, StackMatrix>> &nonZeroBlocks =
        a.get_nonZeroBlocks();

    int quanta_thrds = dmrginp.quanta_thrds();
#pragma omp parallel for schedule(dynamic) num_threads(quanta_thrds)
    for (int index = 0; index < nonZeroBlocks.size(); index++)
        MatrixScale(scale, nonZeroBlocks[index].second);

}
    
void TensorScaleAdd(double scale, const StackSparseMatrix &a, StackSparseMatrix &c,
                    const vector<boost::shared_ptr<StateInfo>> &state_info) {
    
    const StateInfo *brastateinfo, *ketstateinfo;
    
    if (state_info.size() == 1) {
        brastateinfo = ketstateinfo = state_info[0].get();
    } else {
        brastateinfo = state_info[0].get();
        ketstateinfo = state_info[1].get();
    }
    
    assert(c.conjugacy() == 'n');
    
    if (a.conjugacy() == 'n') {
        for (int lQ = 0; lQ < c.nrows(); lQ++)
            for (int rQ = 0; rQ < c.ncols(); rQ++)
                if (c.allowed(lQ, rQ) && a.allowed(lQ, rQ)) {
                    // get_scaling = 1.0 when a.conjugacy() == 'n'
                    // double factor = a.get_scaling(s.quanta[lQ], s.quanta[rQ]);
                    MatrixScaleAdd(scale,
                        a.operator_element(lQ, rQ), c.operator_element(lQ, rQ));
                }
    } else {
        for (int lQ = 0; lQ < c.nrows(); lQ++)
            for (int rQ = 0; rQ < c.ncols(); rQ++)
                if (c.allowed(lQ, rQ) && a.allowed(lQ, rQ)) {

                    double scaling =
                        getStandAlonescaling(a.get_deltaQuantum(0),
                                             brastateinfo->quanta[lQ],
                                             ketstateinfo->quanta[rQ]);

                    int nrows = c.operator_element(lQ, rQ).Nrows();
                    int ncols = c.operator_element(lQ, rQ).Ncols();

                    for (int row = 0; row < nrows; ++row)
                        DAXPY(ncols, scaling * scale,
                              a.operator_element(lQ, rQ).Store() + row, nrows,
                              &(c.operator_element(lQ, rQ)(row + 1, 1)), 1);
                }
    }
}

void TensorScaleAdd(double scale, const StackSparseMatrix &a, StackSparseMatrix &c) {
    assert(a.conjugacy() == 'n' && c.conjugacy() == 'n');
    for (int lQ = 0; lQ < c.nrows(); lQ++)
        for (int rQ = 0; rQ < c.ncols(); rQ++)
            if (c.allowed(lQ, rQ) && a.allowed(lQ, rQ))
                MatrixScaleAdd(scale,
                    a.operator_element(lQ, rQ), c.operator_element(lQ, rQ));
}

double TensorDotProduct(const StackSparseMatrix &a, const StackSparseMatrix &b) {
    assert(a.conjugacy() == 'n' && b.conjugacy() == 'n');
    double result = 0.;
    for (int lQ = 0; lQ < a.nrows(); ++lQ)
        for (int rQ = 0; rQ < a.ncols(); ++rQ)
            if (a.allowed(lQ, rQ) && b.allowed(lQ, rQ))
                result += MatrixDotProduct(a.operator_element(lQ, rQ), b.operator_element(lQ, rQ));

    return result;
}

void TensorPrecondition(StackSparseMatrix &a, double e, const DiagonalMatrix &diag) {
    int index = 1;
    for (int lQ = 0; lQ < a.nrows(); ++lQ)
        for (int rQ = 0; rQ < a.ncols(); ++rQ)
            if (a.allowed(lQ, rQ))
                for (int lQState = 0; lQState < a.operator_element(lQ, rQ).Nrows(); ++lQState)
                    for (int rQState = 0; rQState < a.operator_element(lQ, rQ).Ncols(); ++rQState) {
                        if (fabs(e - diag(index)) > 1E-12)
                            a.operator_element(lQ, rQ)(lQState + 1, rQState + 1)
                                /= (e - diag(index));
                        ++index;
                    }
}
    
} // namespace block2
