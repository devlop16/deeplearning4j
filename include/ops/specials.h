//
// Created by raver119 on 24.04.17.
//

#ifndef LIBND4J_CONCAT_H
#define LIBND4J_CONCAT_H


#ifdef __CUDACC__
#define ELEMENT_THRESHOLD 8192
#define TAD_THRESHOLD 2
#endif


/**
  * Concatneate multi array of the same shape together
  * along a particular dimension
  */
template <typename T>
void concatCpuGeneric(
        int dimension,
        int numArrays,
        Nd4jPointer *data,
        Nd4jPointer *inputShapeInfo,
        T *result,
        int *resultShapeInfo) {
    //number of total arrays, every other dimension should be the same
    T **dataBuffers = reinterpret_cast<T **>(data);
    int **inputShapeInfoPointers = reinterpret_cast<int **>(inputShapeInfo);

    bool allC = true;
    bool allScalar = true;

    //nothing to concat
    if(numArrays == 1)
        return;

    //detect whether all arrays are c ordered or not
    //Also detect whether they are all scalars
    for(int i = 0; i < numArrays; i++) {
        allC &= (shape::order(inputShapeInfoPointers[i]) == 'c');
        allScalar &= (shape::isScalar(inputShapeInfoPointers[i]));
    }

    //we are merging all scalars
    if(allScalar) {
        for(int i = 0; i < numArrays; i++) {
            result[i] = dataBuffers[i][0];
        }
        return;
    }


    int length = shape::length(resultShapeInfo);


    if(allC && dimension == 0 && shape::order(resultShapeInfo) == 'c') {
        int currBuffer = 0;
        int currBufferOffset = 0;
        for(int i = 0; i <  length; i++) {
            result[i] = dataBuffers[currBuffer][currBufferOffset++];
            if(currBufferOffset >= shape::length(inputShapeInfoPointers[currBuffer])) {
                currBuffer++;
                currBufferOffset = 0;
            }
        }

        return;
    }

    int resultStride = shape::elementWiseStride(resultShapeInfo);
    //vector case
    if(shape::isVector(resultShapeInfo)) {
        int idx = 0;
        if(resultStride == 1) {
            for(int i = 0; i < numArrays; i++) {
                if(shape::isVector(inputShapeInfoPointers[i]) || shape::order(inputShapeInfoPointers[i]) == shape::order(resultShapeInfo)) {
                    Nd4jIndex  currArrLength = shape::length(inputShapeInfoPointers[i]);
                    Nd4jIndex eleStride = shape::elementWiseStride(inputShapeInfoPointers[i]);
                    if(eleStride == 1) {
                        for(Nd4jIndex arrIdx = 0; arrIdx < currArrLength; arrIdx++) {
                            if(idx >= shape::length(resultShapeInfo)) {
                                break;
                            }
                            result[idx] = dataBuffers[i][arrIdx];
                            idx++;
                        }
                    }
                    else {
                        for(Nd4jIndex arrIdx = 0; arrIdx < currArrLength; arrIdx++) {
                            result[idx] = dataBuffers[i][arrIdx * eleStride];
                            if(idx >= shape::length(resultShapeInfo)) {
                                break;
                            }

                            idx++;

                        }
                    }
                }
                    //non vector or different order (element wise stride can't be used)
                else {
                    int *coordsUse = new int[shape::rank(inputShapeInfoPointers[i])];
                    Nd4jIndex  currArrLength = shape::length(inputShapeInfoPointers[i]);
                    for(Nd4jIndex arrIdx = 0; arrIdx < currArrLength; arrIdx++) {
                        shape::ind2subC(shape::rank(inputShapeInfoPointers[i]),shape::shapeOf(inputShapeInfoPointers[i]),arrIdx,coordsUse);
                        Nd4jIndex offset = shape::getOffset(0,shape::shapeOf(inputShapeInfoPointers[i]),shape::stride(inputShapeInfoPointers[i]),coordsUse,shape::rank(inputShapeInfoPointers[i]));
                        result[idx] = dataBuffers[i][offset];
                        if(idx >= shape::length(resultShapeInfo)) {
                            break;
                        }

                        idx++;

                    }

                    delete[] coordsUse;
                }


            }
        }
        else {
            for(int i = 0; i < numArrays; i++) {
                if(shape::isVector(inputShapeInfoPointers[i]) || shape::order(inputShapeInfoPointers[i]) == shape::order(resultShapeInfo)) {
                    Nd4jIndex  currArrLength = shape::length(inputShapeInfoPointers[i]);
                    Nd4jIndex eleStride = shape::elementWiseStride(inputShapeInfoPointers[i]);
                    if(eleStride == 1) {
                        for(Nd4jIndex arrIdx = 0; arrIdx < currArrLength; arrIdx++) {
                            if(idx >= shape::length(resultShapeInfo)) {
                                break;
                            }
                            result[idx * resultStride] = dataBuffers[i][arrIdx];
                            idx++;

                        }
                    }
                    else {
                        for(Nd4jIndex arrIdx = 0; arrIdx < currArrLength; arrIdx++) {
                            if(idx >= shape::length(resultShapeInfo)) {
                                break;
                            }
                            result[idx * resultStride] = dataBuffers[i][arrIdx * eleStride];
                            idx++;
                        }
                    }

                }
                    //non vector or different order (element wise stride can't be used)
                else {
                    int *coordsUse = new int[shape::rank(inputShapeInfoPointers[i])];
                    Nd4jIndex  currArrLength = shape::length(inputShapeInfoPointers[i]);

                    for(Nd4jIndex arrIdx = 0; arrIdx < currArrLength; arrIdx++) {
                        shape::ind2subC(shape::rank(inputShapeInfoPointers[i]),shape::shapeOf(inputShapeInfoPointers[i]),arrIdx,coordsUse);
                        Nd4jIndex offset = shape::getOffset(0,shape::shapeOf(inputShapeInfoPointers[i]),shape::stride(inputShapeInfoPointers[i]),coordsUse,shape::rank(inputShapeInfoPointers[i]));
                        result[idx] = dataBuffers[i][offset];
                        if(idx >= shape::length(resultShapeInfo)) {
                            break;
                        }

                        idx++;

                    }

                    delete[] coordsUse;
                }

            }
        }

        return;
    }


    //tad shape information for result
    shape::TAD resultTad(resultShapeInfo,&dimension,1);
    resultTad.createTadOnlyShapeInfo();
    resultTad.createOffsets();
    int resultTadEleStride = shape::elementWiseStride(resultTad.tadOnlyShapeInfo);

    int arrOffset = 0;
    int tadEleStride = shape::elementWiseStride(resultTad.tadOnlyShapeInfo);
    for(int i = 0; i < numArrays; i++) {
        //tad info for the current array
        shape::TAD arrTad(inputShapeInfoPointers[i],&dimension,1);
        arrTad.createTadOnlyShapeInfo();
        arrTad.createOffsets();

        //element wise stride and length for tad of current array
        int arrTadEleStride = shape::elementWiseStride(arrTad.tadOnlyShapeInfo);
        int arrTadLength = shape::length(arrTad.tadOnlyShapeInfo);
        for(int j = 0; j < arrTad.numTads; j++) {
            T *arrTadData = dataBuffers[i] + arrTad.tadOffsets[j];
            //result tad offset + the current offset for each tad + array offset (matches current array)
            T *currResultTadWithOffset = result  + resultTad.tadOffsets[j];
            //ensure we start at the proper index, we need to move the starting index forward relative to the desired array offset
            int* sub = shape::ind2subC(shape::rank(resultTad.tadOnlyShapeInfo),shape::shapeOf(resultTad.tadOnlyShapeInfo),arrOffset);
            Nd4jIndex baseOffset = shape::getOffset(0,shape::shapeOf(resultTad.tadOnlyShapeInfo),shape::stride(resultTad.tadOnlyShapeInfo),sub,shape::rank(resultTad.tadOnlyShapeInfo));
            delete[] sub;
            currResultTadWithOffset += baseOffset;
            if(arrTadEleStride > 0 && shape::order(resultShapeInfo) == shape::order(arrTad.tadOnlyShapeInfo)) {
                if(arrTadEleStride == 1 && resultTadEleStride == 1) {
                    //iterate over the specified chunk of the tad
                    for(int k = 0; k < arrTadLength; k++) {
                        currResultTadWithOffset[k] = arrTadData[k];
                    }

                } //element wise stride isn't 1 for both can't use memcpy
                else if(tadEleStride > 0 && shape::order(resultShapeInfo) == shape::order(arrTad.tadOnlyShapeInfo)) {
                    for(int k = 0; k < arrTadLength; k++) {
                        currResultTadWithOffset[k * tadEleStride] = arrTadData[k * arrTadEleStride];
                    }
                }
            }
            else {
                int idx = 0;
                //use element wise stride for result but not this tad
                if(tadEleStride > 0 && shape::order(resultShapeInfo) == shape::order(arrTad.tadOnlyShapeInfo)) {
                    if(arrTad.wholeThing) {
                        for(int k = 0; k < shape::length(arrTad.tadOnlyShapeInfo); k++) {
                            currResultTadWithOffset[idx *resultTadEleStride] = arrTadData[k];

                        }
                    }
                    else {
                        int shapeIter[MAX_RANK];
                        int coord[MAX_RANK];
                        int dim;
                        int rankIter = shape::rank(arrTad.tadOnlyShapeInfo);
                        int xStridesIter[MAX_RANK];
                        if (PrepareOneRawArrayIter<T>(rankIter,
                                                      shape::shapeOf(arrTad.tadOnlyShapeInfo),
                                                      arrTadData,
                                                      shape::stride(arrTad.tadOnlyShapeInfo),
                                                      &rankIter,
                                                      shapeIter,
                                                      &arrTadData,
                                                      xStridesIter) >= 0) {
                            ND4J_RAW_ITER_START(dim, shape::rank(arrTad.tadOnlyShapeInfo), coord, shapeIter); {
                                /* Process the innermost dimension */
                                currResultTadWithOffset[idx *resultTadEleStride] = arrTadData[0];
                            }
                            ND4J_RAW_ITER_ONE_NEXT(dim,
                                                   rankIter,
                                                   coord,
                                                   shapeIter,
                                                   arrTadData,
                                                   xStridesIter);

                        }
                        else {
                            printf("Unable to prepare array\n");
                        }


                    }

                }
                    //don't use element wise stride for either
                else {

                    int shapeIter[MAX_RANK];
                    int coord[MAX_RANK];
                    int dim;
                    int xStridesIter[MAX_RANK];
                    int resultStridesIter[MAX_RANK];
                    int *xShape = shape::shapeOf(arrTad.tadOnlyShapeInfo);
                    int *xStride = shape::stride(arrTad.tadOnlyShapeInfo);
                    int *resultStride = shape::stride(resultTad.tadOnlyShapeInfo);
                    int rank = shape::rank(arrTad.tadOnlyShapeInfo);
                    if (PrepareTwoRawArrayIter<T>(rank,
                                                  xShape,
                                                  arrTadData,
                                                  xStride,
                                                  currResultTadWithOffset,
                                                  resultStride,
                                                  &rank,
                                                  shapeIter,
                                                  &arrTadData,
                                                  xStridesIter,
                                                  &currResultTadWithOffset,
                                                  resultStridesIter) >= 0) {
                        ND4J_RAW_ITER_START(dim, rank, coord, shapeIter); {
                            currResultTadWithOffset[0] = arrTadData[0];
                        } ND4J_RAW_ITER_TWO_NEXT(
                                dim,
                                rank,
                                coord,
                                shapeIter,
                                arrTadData,
                                xStridesIter,
                                currResultTadWithOffset,
                                resultStridesIter);


                    }
                }
            }

        }

        arrOffset += shape::length(arrTad.tadOnlyShapeInfo);
    }

}


/**
 * This kernel accumulates X arrays, and stores result into Z
 *
 * @tparam T
 * @param x
 * @param z
 * @param n
 * @param length
 */
template<typename T>
void accumulateGeneric(T **x, T *z, int n, const Nd4jIndex length) {
    // aggregation step
#ifdef _OPENNMP
    int _threads = omp_get_max_threads();
#else
    // we can use whatever we want here, this value won't be used if there's no omp
    int _threads = 4;
#endif

#pragma omp parallel for simd num_threads(_threads) schedule(guided) default(shared) proc_bind(close)
    for (int i = 0; i < length; i++) {

        for (int ar = 0; ar < n; ar++) {
            z[i] += x[ar][i];
        }
    }
}


/**
 * This kernel averages X input arrays, and stores result to Z
 *
 * @tparam T
 * @param x
 * @param z
 * @param n
 * @param length
 * @param propagate
 */
template<typename T>
void averageGeneric(T **x, T *z, int n, const Nd4jIndex length, bool propagate) {

    bool tempZ = false;
    if (z == nullptr) {
        z = new T[length];
        if (z == nullptr) {
            printf("Can't allocate temporary array for averaging!\n");
            return;
        }

        tempZ = true;
    }

    // memset before propagation
    memset(z, 0, length * sizeof(T));

    // aggregation step
#ifdef _OPENNMP
    int _threads = omp_get_max_threads(); //nd4j::math::nd4j_min<int>(omp_get_max_threads() / 2, 4);
#else
    // we can use whatever we want here, this value won't be used if there's no omp
    int _threads = 4;
#endif

#pragma omp parallel for simd num_threads(_threads) schedule(guided) default(shared) proc_bind(close)
    for (int i = 0; i < length; i++) {

        for (int ar = 0; ar < n; ar++) {
            z[i] += x[ar][i] / n;
        }
    }


    // instead of doing element-wise propagation, we just issue memcpy to propagate data
#pragma omp parallel for num_threads(_threads) default(shared) proc_bind(close)
    for(int ar = 0; ar < n; ar++) {
        memcpy(x[ar], z, length * sizeof(T));
    }

    if (tempZ)
        delete[] z;
}

int getPosition(int *xShapeInfo, int index) {
    int xEWS = shape::elementWiseStride(xShapeInfo);

    if (xEWS == 1) {
        return index;
    } else if (xEWS > 1) {
        return index * xEWS;
    } else {
        int xCoord[MAX_RANK];
        int xRank = shape::rank(xShapeInfo);
        int *xShape = shape::shapeOf(xShapeInfo);
        int *xStride = shape::stride(xShapeInfo);

        shape::ind2subC(xRank, xShape, index, xCoord);
        int xOffset = shape::getOffset(0, xShape, xStride, xCoord, xRank);

        return xOffset;
    }
}

template<typename T>
void quickSort_parallel_internal(T* array, int *xShapeInfo, int left, int right, int cutoff)
{

    int i = left, j = right;
    T tmp;
    T pivot = array[getPosition(xShapeInfo, (left + right) / 2)];


    {
        /* PARTITION PART */
        while (i <= j) {
            while (array[getPosition(xShapeInfo, i)] < pivot)
                i++;
            while (array[getPosition(xShapeInfo, j)] > pivot)
                j--;
            if (i <= j) {
                tmp = array[getPosition(xShapeInfo, i)];
                array[getPosition(xShapeInfo, i)] = array[getPosition(xShapeInfo, j)];
                array[getPosition(xShapeInfo, j)] = tmp;
                i++;
                j--;
            }
        }

    }


    if ( ((right-left)<cutoff) ){
        if (left < j){ quickSort_parallel_internal(array, xShapeInfo, left, j, cutoff); }
        if (i < right){ quickSort_parallel_internal(array, xShapeInfo, i, right, cutoff); }

    }else{
#pragma omp task
        { quickSort_parallel_internal(array, xShapeInfo, left, j, cutoff); }
#pragma omp task
        { quickSort_parallel_internal(array, xShapeInfo, i, right, cutoff); }
    }

}

template<typename T>
void quickSort_parallel(T* array, int *xShapeInfo, int lenArray, int numThreads){

    int cutoff = 1000;

#pragma omp parallel num_threads(numThreads)
    {
#pragma omp single nowait
        {
            quickSort_parallel_internal(array, xShapeInfo, 0, lenArray-1, cutoff);
        }
    }

}


template<typename T>
void sortGeneric(T *x, int *xShapeInfo, bool descending) {
    quickSort_parallel(x, xShapeInfo, shape::length(xShapeInfo), omp_get_max_threads());
}


#endif //LIBND4J_CONCAT_H
