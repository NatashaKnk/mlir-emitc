//===- MHLOToEmitC.cpp - MHLO to EmitC conversion ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements logic for lowering MHLO dialect to EmitC dialect.
//
//===----------------------------------------------------------------------===//

#include "emitc/Dialect/EmitC/EmitCDialect.h"
#include "emitc/Dialect/EmitC/Passes.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "mlir/IR/Function.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

namespace mlir {
namespace emitc {

namespace {

// Adopted from IREE's ConvertStandardToVM/ConvertVMToEmitC.
template <typename SrcOp, typename DstOp>
class BinaryOpConversion : public OpConversionPattern<SrcOp> {
  using OpConversionPattern<SrcOp>::OpConversionPattern;

public:
  BinaryOpConversion(MLIRContext *ctx, StringRef funcName)
      : OpConversionPattern<SrcOp>(ctx), funcName(funcName) {}

private:
  LogicalResult
  matchAndRewrite(SrcOp srcOp, ArrayRef<Value> operands,
                  ConversionPatternRewriter &rewriter) const override {
    typename SrcOp::Adaptor srcAdapter(operands);

    StringAttr callee = rewriter.getStringAttr(funcName);
    ArrayAttr args =
        rewriter.getArrayAttr({IntegerAttr::get(rewriter.getIndexType(), 0),
                               IntegerAttr::get(rewriter.getIndexType(), 1)});
    ValueRange dstOperands{srcAdapter.lhs(), srcAdapter.rhs()};

    rewriter.replaceOpWithNewOp<DstOp>(srcOp, srcAdapter.lhs().getType(),
                                       callee, args, dstOperands);

    return success();
  }

  StringRef funcName;
};

} // namespace

void populateMhloToEmitcPatterns(MLIRContext *ctx,
                                 OwningRewritePatternList &patterns) {
  patterns.insert<BinaryOpConversion<mhlo::AddOp, emitc::CallOp>>(
      ctx, "std::plus<>{}");
  patterns.insert<BinaryOpConversion<mhlo::SubOp, emitc::CallOp>>(
      ctx, "std::minus<>{}");
  patterns.insert<BinaryOpConversion<mhlo::MulOp, emitc::CallOp>>(
      ctx, "std::multiplies<>{}");
  patterns.insert<BinaryOpConversion<mhlo::DivOp, emitc::CallOp>>(
      ctx, "std::divides<>{}");
}

namespace {

struct ConvertMhloToEmitcPass
    : public PassWrapper<ConvertMhloToEmitcPass, FunctionPass> {
  /// Perform the lowering to EmitC dialect.
  void runOnFunction() override {

    ConversionTarget target(getContext());

    target.addLegalDialect<emitc::EmitCDialect>();
    target.addLegalDialect<mhlo::MhloDialect>();
    target.addIllegalOp<mhlo::AddOp, mhlo::SubOp, mhlo::MulOp, mhlo::DivOp>();

    OwningRewritePatternList patterns;
    populateMhloToEmitcPatterns(&getContext(), patterns);

    if (failed(applyPartialConversion(getFunction(), target, patterns)))
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<mlir::OperationPass<mlir::FuncOp>>
createConvertMhloToEmitcPass() {
  return std::make_unique<ConvertMhloToEmitcPass>();
}

} // namespace emitc
} // namespace mlir
