//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Bufferization/IR/BufferizableOpInterface.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Bufferization/IR/BufferizationTypeInterfaces.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Transforms/InliningUtils.h"

using namespace mlir;
using namespace mlir::bufferization;

#include "mlir/Dialect/Bufferization/IR/BufferizationOpsDialect.cpp.inc"

/// Attribute name used to mark function arguments who's buffers can be written
/// to during One-Shot Module Bufferize.
constexpr const ::llvm::StringLiteral BufferizationDialect::kWritableAttrName;

/// Attribute name used to mark the bufferization layout for region arguments
/// during One-Shot Module Bufferize.
constexpr const ::llvm::StringLiteral
    BufferizationDialect::kBufferLayoutAttrName;

/// An attribute that can be attached to ops with an allocation and/or
/// deallocation side effect. It indicates that the op is under a "manual
/// deallocation" scheme. In the case of an allocation op, the returned
/// value is *not* an automatically managed allocation and assigned an
/// ownership of "false". Furthermore, only deallocation ops that are
/// guaranteed to deallocate a buffer under "manual deallocation" are
/// allowed to have this attribute. (Deallocation ops without this
/// attribute are rejected by the ownership-based buffer deallocation pass.)
constexpr const ::llvm::StringLiteral BufferizationDialect::kManualDeallocation;

//===----------------------------------------------------------------------===//
// Bufferization Dialect Interfaces
//===----------------------------------------------------------------------===//

namespace {
struct BufferizationInlinerInterface : public DialectInlinerInterface {
  using DialectInlinerInterface::DialectInlinerInterface;

  /// Operations in Bufferization dialect are always legal to inline.
  bool isLegalToInline(Operation *, Region *, bool, IRMapping &) const final {
    return true;
  }
};

template <typename Tensor>
struct BuiltinTensorExternalModel
    : TensorLikeType::ExternalModel<BuiltinTensorExternalModel<Tensor>,
                                    Tensor> {
  llvm::FailureOr<BufferLikeType> getBufferType(
      mlir::Type tensor, const BufferizationOptions &options,
      llvm::function_ref<mlir::InFlightDiagnostic()> emitError) const {
    auto tensorType = cast<TensorType>(tensor);
    auto memSpace = options.defaultMemorySpaceFn(tensorType);
    if (!memSpace.has_value())
      return emitError() << "could not infer memory space";

    return cast<BufferLikeType>(
        getMemRefType(tensorType, options, /*layout=*/{}, *memSpace));
  }

  mlir::LogicalResult verifyCompatibleBufferType(
      mlir::Type tensor, BufferLikeType bufferType,
      llvm::function_ref<mlir::InFlightDiagnostic()> emitError) const {
    assert(isa<TensorType>(tensor) && "expected tensor type");
    assert(isa<BaseMemRefType>(bufferType) && "expected memref type");

    auto tensorType = cast<ShapedType>(tensor);
    auto memrefType = cast<ShapedType>(bufferType);

    if (tensorType.getShape() != memrefType.getShape())
      return emitError() << "shapes do not match";

    if (tensorType.getElementType() != memrefType.getElementType())
      return emitError() << "element types do not match";

    return mlir::success();
  }
};

template <typename MemRef>
struct BuiltinMemRefExternalModel
    : BufferLikeType::ExternalModel<BuiltinMemRefExternalModel<MemRef>,
                                    MemRef> {};
} // namespace

//===----------------------------------------------------------------------===//
// Bufferization Dialect
//===----------------------------------------------------------------------===//

void mlir::bufferization::BufferizationDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "mlir/Dialect/Bufferization/IR/BufferizationOps.cpp.inc"
      >();
  addInterfaces<BufferizationInlinerInterface>();

  // Note: Unlike with other external models, declaring bufferization's
  // "promised interfaces" in builtins for TensorLike and BufferLike type
  // interfaces is not possible (due to builtins being independent of
  // bufferization). Thus, the compromise is to attach these interfaces directly
  // during dialect initialization.
  RankedTensorType::attachInterface<
      BuiltinTensorExternalModel<RankedTensorType>>(*getContext());
  UnrankedTensorType::attachInterface<
      BuiltinTensorExternalModel<UnrankedTensorType>>(*getContext());
  MemRefType::attachInterface<BuiltinMemRefExternalModel<MemRefType>>(
      *getContext());
  UnrankedMemRefType::attachInterface<
      BuiltinMemRefExternalModel<UnrankedMemRefType>>(*getContext());
}

LogicalResult BufferizationDialect::verifyRegionArgAttribute(
    Operation *op, unsigned /*regionIndex*/, unsigned argIndex,
    NamedAttribute attr) {
  if (attr.getName() == kWritableAttrName) {
    if (!llvm::isa<BoolAttr>(attr.getValue())) {
      return op->emitError() << "'" << kWritableAttrName
                             << "' is expected to be a boolean attribute";
    }
    if (!isa<FunctionOpInterface>(op))
      return op->emitError() << "expected '" << kWritableAttrName
                             << "' to be used on function-like operations";
    if (cast<FunctionOpInterface>(op).isExternal())
      return op->emitError() << "'" << kWritableAttrName
                             << "' is invalid on external functions";
    return success();
  }
  if (attr.getName() == kBufferAccessAttrName) {
    if (!llvm::isa<StringAttr>(attr.getValue())) {
      return op->emitError() << "'" << kBufferAccessAttrName
                             << "' is expected to be a string attribute";
    }
    StringRef str = llvm::cast<StringAttr>(attr.getValue()).getValue();
    if (str != "none" && str != "read" && str != "write" && str != "read-write")
      return op->emitError()
             << "invalid value for '" << kBufferAccessAttrName << "'";
    if (!isa<FunctionOpInterface>(op))
      return op->emitError() << "expected '" << kBufferAccessAttrName
                             << "' to be used on function-like operations";
    return success();
  }
  if (attr.getName() == kBufferLayoutAttrName) {
    if (!llvm::isa<MemRefLayoutAttrInterface>(attr.getValue())) {
      return op->emitError() << "'" << kBufferLayoutAttrName
                             << "' is expected to be a memref layout attribute";
    }
    if (!isa<FunctionOpInterface>(op))
      return op->emitError() << "expected '" << kBufferLayoutAttrName
                             << "' to be used on function-like operations";
    return success();
  }
  return op->emitError() << "attribute '" << kBufferLayoutAttrName
                         << "' not supported as a region arg attribute by the "
                            "bufferization dialect";
}

LogicalResult
BufferizationDialect::verifyOperationAttribute(Operation *op,
                                               NamedAttribute attr) {
  using bufferization::BufferizableOpInterface;

  if (attr.getName() == kManualDeallocation) {
    if (!mlir::hasEffect<MemoryEffects::Allocate>(op) &&
        !mlir::hasEffect<MemoryEffects::Free>(op))
      return op->emitOpError("attribute '")
             << kManualDeallocation
             << "' can be used only on ops that have an allocation and/or free "
                "side effect";
    return success();
  }

  return op->emitError()
         << "attribute '" << attr.getName()
         << "' not supported as an op attribute by the bufferization dialect";
}
