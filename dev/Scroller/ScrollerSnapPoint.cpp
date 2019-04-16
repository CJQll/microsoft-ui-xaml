﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "common.h"
#include "TypeLogging.h"
#include "ScrollerSnapPoint.h"

// Required for Modern Idl bug, should never be called.
SnapPointBase::SnapPointBase()
{
    // throw (ERROR_CALL_NOT_IMPLEMENTED);
}

winrt::hstring SnapPointBase::GetTargetExpression(winrt::hstring const& target) const
{
    return StringUtil::FormatString(L"this.Target.%1!s!", target.data());
}

bool SnapPointBase::operator<(SnapPointBase* snapPoint)
{
    ScrollerSnapPointSortPredicate mySortPredicate = SortPredicate();
    ScrollerSnapPointSortPredicate theirSortPredicate = snapPoint->SortPredicate();
    if (mySortPredicate.primary < theirSortPredicate.primary)
    {
        return true;
    } 
    if (theirSortPredicate.primary < mySortPredicate.primary)
    {
        return false;
    }

    if (mySortPredicate.secondary < theirSortPredicate.secondary)
    {
        return true;
    }
    if (theirSortPredicate.secondary < mySortPredicate.secondary)
    {
        return false;
    }

    if (mySortPredicate.tertiary < theirSortPredicate.tertiary)
    {
        return true;
    }
    return false;
}

bool SnapPointBase::operator==(SnapPointBase* snapPoint)
{
    ScrollerSnapPointSortPredicate mySortPredicate = SortPredicate();
    ScrollerSnapPointSortPredicate theirSortPredicate = snapPoint->SortPredicate();
    if (std::abs(mySortPredicate.primary - theirSortPredicate.primary) < s_equalityEpsilon
        && std::abs(mySortPredicate.secondary - theirSortPredicate.secondary) < s_equalityEpsilon
        && mySortPredicate.tertiary == theirSortPredicate.tertiary)
    {
        return true;
    }
    return false;
}

#ifdef ApplicableRangeType
double SnapPointBase::ApplicableRange()
{
    return m_specifiedApplicableRange;
}

winrt::SnapPointApplicableRangeType SnapPointBase::ApplicableRangeType()
{
    return m_applicableRangeType;
}
#endif

#ifdef _DEBUG
winrt::Color SnapPointBase::VisualizationColor()
{
    return m_visualizationColor;
}

void SnapPointBase::VisualizationColor(winrt::Color color)
{       
    m_visualizationColor = color;
}
#endif // _DEBUG

bool SnapPointBase::SnapsAt(
    std::tuple<double, double> actualApplicableZone,
    double value) const
{
    if (std::get<0>(actualApplicableZone) <= value &&
        std::get<1>(actualApplicableZone) >= value)
    {
        double snappedValue = Evaluate(actualApplicableZone, static_cast<float>(value));

        return std::abs(value - snappedValue) < s_equalityEpsilon;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////
/////////////////      Scroll Snap Points     ///////////////////////
/////////////////////////////////////////////////////////////////////

// Required for Modern Idl bug, should never be called.
ScrollSnapPointBase::ScrollSnapPointBase()
{
    // throw (ERROR_CALL_NOT_IMPLEMENTED);
}

winrt::ScrollSnapPointsAlignment ScrollSnapPointBase::Alignment()
{
    return m_alignment;
}

// Returns True when this snap point is sensitive to the viewport size and is interested in future updates.
bool ScrollSnapPointBase::OnUpdateViewport(double newViewport)
{
    switch (m_alignment)
    {
    case winrt::ScrollSnapPointsAlignment::Near:
        MUX_ASSERT(m_alignmentAdjustment == 0.0);
        return false;
    case winrt::ScrollSnapPointsAlignment::Center:
        m_alignmentAdjustment = -newViewport / 2.0;
        break;
    case winrt::ScrollSnapPointsAlignment::Far:
        m_alignmentAdjustment = -newViewport;
        break;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////
//////////////    Irregular Scroll Snap Points   ////////////////////
/////////////////////////////////////////////////////////////////////
CppWinRTActivatableClassWithBasicFactory(ScrollSnapPoint);

ScrollSnapPoint::ScrollSnapPoint(
    double snapPointValue,
    winrt::ScrollSnapPointsAlignment alignment)
{
    m_value = snapPointValue;
    m_alignment = alignment;
}

#ifdef ApplicableRangeType
ScrollSnapPoint::ScrollSnapPoint(
    double snapPointValue,
    double applicableRange,
    winrt::ScrollSnapPointsAlignment alignment)
{
    if (applicableRange <= 0)
    {
        throw winrt::hresult_invalid_argument(L"'applicableRange' must be strictly positive.");
    }

    m_value = snapPointValue;
    m_alignment = alignment;
    m_specifiedApplicableRange = applicableRange;
    m_actualApplicableZone = std::tuple<double, double>{ snapPointValue - applicableRange, snapPointValue + applicableRange};
    m_applicableRangeType = winrt::SnapPointApplicableRangeType::Optional;
}
#endif

double ScrollSnapPoint::Value()
{
    return m_value;
}

winrt::ExpressionAnimation ScrollSnapPoint::CreateRestingPointExpression(
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    winrt::hstring expression = StringUtil::FormatString(L"snapPointValue * %1!s!", scale.data());
    winrt::ExpressionAnimation restingPointExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    restingPointExpressionAnimation.SetScalarParameter(L"snapPointValue", static_cast<float>(ActualValue()));
    return restingPointExpressionAnimation;
}

winrt::ExpressionAnimation ScrollSnapPoint::CreateConditionalExpression(
    std::tuple<double, double> actualApplicableZone,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    winrt::hstring targetExpression = GetTargetExpression(target);
    winrt::hstring scaledMinApplicableRange = StringUtil::FormatString(
        L"(minApplicableValue * %1!s!)",
        scale.data());
    winrt::hstring scaledMaxApplicableRange = StringUtil::FormatString(
        L"(maxApplicableValue * %1!s!)",
        scale.data());
    winrt::hstring scaledMinImpulseApplicableRange = StringUtil::FormatString(
        L"(minImpulseApplicableValue * %1!s!)",
        scale.data());
    winrt::hstring scaledMaxImpulseApplicableRange = StringUtil::FormatString(
        L"(maxImpulseApplicableValue * %1!s!)",
        scale.data());
    winrt::hstring expression = StringUtil::FormatString(
        L"this.Target.IsInertiaFromImpulse ? (%1!s! >= %4!s! && %1!s! <= %5!s!) : (%1!s! >= %2!s! && %1!s! <= %3!s!)",
        targetExpression.data(),
        scaledMinApplicableRange.data(),
        scaledMaxApplicableRange.data(),
        scaledMinImpulseApplicableRange.data(),
        scaledMaxImpulseApplicableRange.data());
    winrt::ExpressionAnimation conditionExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    conditionExpressionAnimation.SetScalarParameter(L"minApplicableValue", static_cast<float>(std::get<0>(actualApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"maxApplicableValue", static_cast<float>(std::get<1>(actualApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"minImpulseApplicableValue", static_cast<float>(std::get<0>(actualImpulseApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"maxImpulseApplicableValue", static_cast<float>(std::get<1>(actualImpulseApplicableZone)));
    return conditionExpressionAnimation;
}

void ScrollSnapPoint::UpdateConditionalExpressionAnimationForImpulse(
    winrt::ExpressionAnimation conditionExpressionAnimation,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
}

void ScrollSnapPoint::UpdateRestingPointExpressionAnimationForImpulse(
    winrt::ExpressionAnimation restingValueExpressionAnimation,
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
}

ScrollerSnapPointSortPredicate ScrollSnapPoint::SortPredicate()
{
    double actualValue = ActualValue();

    // Irregular snap point should be sorted before repeated snap points so it gives a tertiary sort value of 0 (repeated snap points get 1)
    return ScrollerSnapPointSortPredicate{ actualValue, actualValue, 0 };
}

std::tuple<double, double> ScrollSnapPoint::DetermineActualApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint)
{
    return std::tuple<double, double>{
        DetermineMinActualApplicableZone(previousSnapPoint),
        DetermineMaxActualApplicableZone(nextSnapPoint) };
}

std::tuple<double, double> ScrollSnapPoint::DetermineActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue,
    double nextIgnoredValue)
{
    return std::tuple<double, double>{
        DetermineMinActualImpulseApplicableZone(
            previousSnapPoint,
            currentIgnoredValue,
            previousIgnoredValue),
        DetermineMaxActualImpulseApplicableZone(
            nextSnapPoint,
            currentIgnoredValue,
            nextIgnoredValue) };
}

double ScrollSnapPoint::ActualValue() const
{
    return m_value + m_alignmentAdjustment;
}

double ScrollSnapPoint::DetermineMinActualApplicableZone(
    SnapPointBase* previousSnapPoint) const
{
    // If we are not passed a previousSnapPoint it means we are the first in the list, see if we expand to negative Infinity or stay put.
    if (!previousSnapPoint)
    {
#ifdef ApplicableRangeType
        if (applicableRangeType != winrt::SnapPointApplicableRangeType::Optional)
        {
            return -INFINITY;
        }
        else
        {
            return ActualValue() - m_specifiedApplicableRange;
        }
#else
        return -INFINITY;
#endif
    }
    // If we are passed a previousSnapPoint then we need to account for its influence on us.
    else
    {
        double previousMaxInfluence = previousSnapPoint->Influence(ActualValue());

#ifdef ApplicableRangeType
        switch (m_applicableRangeType)
        {
        case winrt::SnapPointApplicableRangeType::Optional:
            return std::max(previousMaxInfluence, ActualValue() - m_specifiedApplicableRange);
        case winrt::SnapPointApplicableRangeType::Mandatory:
            return previousMaxInfluence;
        default:
            MUX_ASSERT(false);
            return 0.0;
        }
#else
        return previousMaxInfluence;
#endif
    }
}

double ScrollSnapPoint::DetermineMinActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue) const
{
    if (!previousSnapPoint)
    {
        return -INFINITY;
    }
    else
    {
        double previousMaxInfluence = previousSnapPoint->ImpulseInfluence(ActualValue(), previousIgnoredValue);

        if (isnan(currentIgnoredValue))
        {
            return previousMaxInfluence;
        }
        else
        {
            return std::max(previousMaxInfluence, ActualValue());
        }
    }
}

double ScrollSnapPoint::DetermineMaxActualApplicableZone(
    SnapPointBase* nextSnapPoint) const
{
    // If we are not passed a nextSnapPoint it means we are the last in the list, see if we expand to Infinity or stay put.
    if (!nextSnapPoint)
    {
#ifdef ApplicableRangeType
        if (m_applicableRangeType != winrt::SnapPointApplicableRangeType::Optional)
        {
            return INFINITY;
        }
        else
        {
            return ActualValue() + m_specifiedApplicableRange;
        }
#else
        return INFINITY;
#endif
    }
    // If we are passed a nextSnapPoint then we need to account for its influence on us.
    else
    {
        double nextMinInfluence = nextSnapPoint->Influence(ActualValue());

#ifdef ApplicableRangeType
        switch (m_applicableRangeType)
        {
        case winrt::SnapPointApplicableRangeType::Optional:
            return std::min(ActualValue() + m_specifiedApplicableRange, nextMinInfluence);
        case winrt::SnapPointApplicableRangeType::Mandatory:
            return nextMinInfluence;
        default:
            MUX_ASSERT(false);
            return 0.0;
        }
#else
        return nextMinInfluence;
#endif
    }
}

double ScrollSnapPoint::DetermineMaxActualImpulseApplicableZone(
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double nextIgnoredValue) const
{
    if (!nextSnapPoint)
    {
        return INFINITY;
    }
    else
    {
        double nextMinInfluence = nextSnapPoint->ImpulseInfluence(ActualValue(), nextIgnoredValue);

        if (isnan(currentIgnoredValue))
        {
            return nextMinInfluence;
        }
        else
        {
            return std::min(ActualValue(), nextMinInfluence);
        }
    }
}

double ScrollSnapPoint::Influence(double edgeOfMidpoint) const
{
    double actualValue = ActualValue();
    double midPoint = (actualValue + edgeOfMidpoint) / 2;

#ifdef ApplicableRangeType
    switch (m_applicableRangeType)
    {
    case winrt::SnapPointApplicableRangeType::Optional:
        if (actualValue <= edgeOfMidpoint)
        {
            return std::min(actualValue + m_specifiedApplicableRange, midPoint);
        }
        else
        {
            return std::max(actualValue - m_specifiedApplicableRange, midPoint);
        }
    case winrt::SnapPointApplicableRangeType::Mandatory:
        return midPoint;
    default:
        MUX_ASSERT(false);
        return 0.0;
    }
#else
    return midPoint;
#endif
}

double ScrollSnapPoint::ImpulseInfluence(double edgeOfMidpoint, double ignoredValue) const
{
    double actualValue = ActualValue();
    double midPoint = (actualValue + edgeOfMidpoint) / 2.0;

    if (isnan(ignoredValue))
    {
        return midPoint;
    }
    else
    {
        if (actualValue <= edgeOfMidpoint)
        {
            return std::min(actualValue, midPoint);
        }
        else
        {
            return std::max(actualValue, midPoint);
        }
    }
}

void ScrollSnapPoint::Combine(
    int& combinationCount,
    winrt::SnapPointBase const& snapPoint) const
{
    auto snapPointAsIrregular = snapPoint.try_as<winrt::ScrollSnapPoint>();
    if (snapPointAsIrregular)
    {
#ifdef ApplicableRangeType
        //TODO: The m_specifiedApplicableRange field is never expected to change after creation. A correction will be needed here.
        m_specifiedApplicableRange = std::max(snapPointAsIrregular.ApplicableRange(), m_specifiedApplicableRange);
#else
        MUX_ASSERT(m_specifiedApplicableRange == INFINITY);
#endif
        combinationCount++;
    }
    else
    {
        // TODO: Provide custom error message
        throw winrt::hresult_error(E_INVALIDARG);
    }
}

int ScrollSnapPoint::SnapCount() const
{
    return 1;
}

double ScrollSnapPoint::Evaluate(
    std::tuple<double, double> actualApplicableZone,
    double value) const
{
    if (value >= std::get<0>(actualApplicableZone) && value <= std::get<1>(actualApplicableZone))
    {
        return ActualValue();
    }
    return value;
}

/////////////////////////////////////////////////////////////////////
/////////////////    Repeated Snap Points    ////////////////////////
/////////////////////////////////////////////////////////////////////
CppWinRTActivatableClassWithBasicFactory(RepeatedScrollSnapPoint);

RepeatedScrollSnapPoint::RepeatedScrollSnapPoint(
    double offset,
    double interval,
    double start,
    double end,
    winrt::ScrollSnapPointsAlignment alignment)
{
    ValidateConstructorParameters(
#ifdef ApplicableRangeType
        false /*applicableRangeToo*/,
        0 /*applicableRange*/,
#endif
        offset,
        interval,
        start,
        end);

    m_offset = offset;
    m_interval = interval;
    m_start = start;
    m_end = end;
    m_alignment = alignment;
}

#ifdef ApplicableRangeType
RepeatedScrollSnapPoint::RepeatedScrollSnapPoint(
    double offset,
    double interval,
    double start,
    double end,
    double applicableRange,
    winrt::ScrollSnapPointsAlignment alignment)
{
    ValidateConstructorParameters(
        true /*applicableRangeToo*/,
        applicableRange,
        offset,
        interval,
        start,
        end);
    
    m_offset = offset;
    m_interval = interval;
    m_start = start;
    m_end = end;
    m_specifiedApplicableRange = applicableRange;
    m_applicableRangeType = winrt::SnapPointApplicableRangeType::Optional;
    m_alignment = alignment;
}
#endif

double RepeatedScrollSnapPoint::Offset()
{
    return m_offset;
}

double RepeatedScrollSnapPoint::Interval()
{
    return m_interval;
}

double RepeatedScrollSnapPoint::Start()
{
    return m_start;
}

double RepeatedScrollSnapPoint::End()
{
    return m_end;
}

winrt::ExpressionAnimation RepeatedScrollSnapPoint::CreateRestingPointExpression(
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    /*
    fracTarget = (target / scale - first) / interval       // Unsnapped value in fractional unscaled intervals from first snapping value
    prevSnap = ((Floor(fracTarget) * interval) + first)    // First unscaled snapped value before unsnapped value
    nextSnap = ((Ceil(fracTarget) * interval) + first)     // First unscaled snapped value after unsnapped value
    effectiveEnd = (IsInertiaFromImpulse ? impEnd : end)   // Regular or impulse upper bound of unscaled applicable zone
     
    Expression:
     ((Abs(target / scale - prevSnap) >= Abs(target / scale - nextSnap)) && (nextSnap <= effectiveEnd))
     ?
     // nextSnap value is closer to unsnapped value and within applicable zone.
     (
      IsInertiaFromImpulse
      ?
      // Impulse mode.
      (
       nextSnap == impIgn
       ?
       (
        // Next snapped value is ignored. Pick the previous snapped value if any, else the ignored value.
        (impIgn == first ? first * scale : (impIgn - interval) * scale)
       )
       :
       // Pick next snapped value.
       nextSnap * scale
      )
      :
      // Regular mode. Pick next snapped value.
      nextSnap * scale
     )
     :
     // prevSnap value is closer to unsnapped value.
     (
      IsInertiaFromImpulse
      ?
      // Impulse mode.
      (
       prevSnap == impIgn
       ?
       // Previous snapped value is ignored. Pick the next snapped value if any, else the ignored value.
       (impIgn + interval <= effectiveEnd ? (impIgn + interval) * scale : impIgn * scale)
       :
       // Pick previous snapped value.
       prevSnap * scale
      )
      :
      // Regular mode. Pick previous snapped value.
      prevSnap * scale
     )
    */

    winrt::hstring expression = StringUtil::FormatString(
        L"((Abs(it.%1!s!/it.Scale-((Floor((it.%1!s!/it.Scale-fst)/itv)*itv)+fst))>=Abs(it.%1!s!/it.Scale-((Ceil((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)))&&(((Ceil((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)<=(it.IsInertiaFromImpulse?iEnd:end)))?(it.IsInertiaFromImpulse?(((Ceil((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)==iIgn?((iIgn==fst?fst*it.Scale:(iIgn-itv)*it.Scale)):((Ceil((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)*it.Scale):((Ceil((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)*it.Scale):(it.IsInertiaFromImpulse?(((Floor((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)==iIgn?(iIgn+itv<=(it.IsInertiaFromImpulse?iEnd:end)?(iIgn+itv)*it.Scale:iIgn*it.Scale):((Floor((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)*it.Scale):((Floor((it.%1!s!/it.Scale-fst)/itv)*itv)+fst)*it.Scale)",
        target.data());

    winrt::ExpressionAnimation restingPointExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    restingPointExpressionAnimation.SetScalarParameter(L"itv", static_cast<float>(m_interval));
    restingPointExpressionAnimation.SetScalarParameter(L"end", static_cast<float>(ActualEnd()));
    restingPointExpressionAnimation.SetScalarParameter(L"fst", static_cast<float>(DetermineFirstRepeatedSnapPointValue()));
    restingPointExpressionAnimation.SetReferenceParameter(L"it", interactionTracker);

    UpdateRestingPointExpressionAnimationForImpulse(
        restingPointExpressionAnimation,
        ignoredValue,
        actualImpulseApplicableZone);

    return restingPointExpressionAnimation;
}

winrt::ExpressionAnimation RepeatedScrollSnapPoint::CreateConditionalExpression(
    std::tuple<double, double> actualApplicableZone,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    MUX_ASSERT(std::get<0>(actualApplicableZone) == ActualStart());
    MUX_ASSERT(std::get<1>(actualApplicableZone) == ActualEnd());

    /*
    fracTarget = (target / scale - first) / interval       // Unsnapped value in fractional unscaled intervals from first snapping value
    prevSnap = ((Floor(fracTarget) * interval) + first)    // First unscaled snapped value before unsnapped value
    nextSnap = ((Ceil(fracTarget) * interval) + first)     // First unscaled snapped value after unsnapped value
    effectiveEnd = (IsInertiaFromImpulse ? impEnd : end)   // Regular or impulse upper bound of unscaled applicable zone

    Expression:
    (
     (!IsInertiaFromImpulse && target / scale >= start && target / scale <= end)       // If we are within the start and end in non-impulse mode
     ||
     (IsInertiaFromImpulse && target / scale >= impStart && target / scale <= impEnd)  // or we are within the impulse start and end in impulse mode
    )
    &&                                                                                 // and...
    (                                                                                  // The location of the repeated snap point just before the natural resting point
     (prevSnap + appRange >= target / scale)                                           // Plus the applicable range is greater than the natural resting point
     ||                                                                                // or...
     (                                                                                 // The location of the repeated snap point just after the natural resting point
      (nextSnap - appRange <= target / scale) &&                                       // Minus the applicable range is less than the natural resting point.
      (nextSnap <= effectiveEnd)                                                       // And the snap point after the natural resting point is less than or equal to the effective end value
     )
    )
    */

    winrt::hstring expression = StringUtil::FormatString(
        L"((!it.IsInertiaFromImpulse && it.%1!s! / it.Scale >= stt && it.%1!s! / it.Scale <= end) || (it.IsInertiaFromImpulse && it.%1!s! / it.Scale >= iStt && it.%1!s! / it.Scale <= iEnd)) && (((Floor((it.%1!s! / it.Scale - fst) / itv) * itv) + fst + aRg >= it.%1!s! / it.Scale) || (((Ceil((it.%1!s! / it.Scale - fst) / itv) * itv) + fst - aRg <= it.%1!s! / it.Scale) && ((Ceil((it.%1!s! / it.Scale - fst) / itv) * itv) + fst <= (it.IsInertiaFromImpulse ? iEnd : end))))",
        target.data());
    winrt::ExpressionAnimation conditionExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    conditionExpressionAnimation.SetScalarParameter(L"itv", static_cast<float>(m_interval));
    conditionExpressionAnimation.SetScalarParameter(L"fst", static_cast<float>(DetermineFirstRepeatedSnapPointValue()));
    conditionExpressionAnimation.SetScalarParameter(L"stt", static_cast<float>(ActualStart()));
    conditionExpressionAnimation.SetScalarParameter(L"end", static_cast<float>(ActualEnd()));
    conditionExpressionAnimation.SetScalarParameter(L"aRg", static_cast<float>(m_specifiedApplicableRange));
    conditionExpressionAnimation.SetReferenceParameter(L"it", interactionTracker);

    UpdateConditionalExpressionAnimationForImpulse(
        conditionExpressionAnimation,
        actualImpulseApplicableZone);

    return conditionExpressionAnimation;
}

void RepeatedScrollSnapPoint::UpdateConditionalExpressionAnimationForImpulse(
    winrt::ExpressionAnimation conditionExpressionAnimation,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
    conditionExpressionAnimation.SetScalarParameter(L"iStt", static_cast<float>(std::get<0>(actualImpulseApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"iEnd", static_cast<float>(std::get<1>(actualImpulseApplicableZone)));
}

void RepeatedScrollSnapPoint::UpdateRestingPointExpressionAnimationForImpulse(
    winrt::ExpressionAnimation restingValueExpressionAnimation,
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
    restingValueExpressionAnimation.SetScalarParameter(L"iEnd", static_cast<float>(std::get<1>(actualImpulseApplicableZone)));
    restingValueExpressionAnimation.SetScalarParameter(L"iIgn", static_cast<float>(ActualImpulseIgnoredValue(ignoredValue)));
}

ScrollerSnapPointSortPredicate RepeatedScrollSnapPoint::SortPredicate()
{
    // Repeated snap points should be sorted after irregular snap points, so give it a tertiary sort value of 1 (irregular snap points get 0)
    return ScrollerSnapPointSortPredicate{ ActualStart(), ActualEnd(), 1 };
}

std::tuple<double, double> RepeatedScrollSnapPoint::DetermineActualApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint)
{
    std::tuple<double, double> actualApplicableZoneReturned = std::tuple<double, double>{
        DetermineMinActualApplicableZone(previousSnapPoint),
        DetermineMaxActualApplicableZone(nextSnapPoint) };

    // Influence() will not have thrown if either of the adjacent snap points are also repeated snap points which have the same start and end, however this is not allowed.
    // We only need to check the nextSnapPoint because of the symmetry in the algorithm.
    if (nextSnapPoint && *static_cast<SnapPointBase*>(this) == (nextSnapPoint))
    {
        // TODO: Provide custom error message
        throw winrt::hresult_error(E_INVALIDARG);
    }

    return actualApplicableZoneReturned;
}

std::tuple<double, double> RepeatedScrollSnapPoint::DetermineActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue,
    double nextIgnoredValue)
{
    return std::tuple<double, double>{
        DetermineMinActualImpulseApplicableZone(
            previousSnapPoint,
            currentIgnoredValue,
            previousIgnoredValue),
        DetermineMaxActualImpulseApplicableZone(
            nextSnapPoint,
            currentIgnoredValue,
            nextIgnoredValue) };
}

double RepeatedScrollSnapPoint::ActualOffset() const
{
    return m_offset + m_alignmentAdjustment;
}

double RepeatedScrollSnapPoint::ActualStart() const
{
    return m_start + m_alignmentAdjustment;
}

double RepeatedScrollSnapPoint::ActualEnd() const
{
    return m_end + m_alignmentAdjustment;
}

double RepeatedScrollSnapPoint::ActualImpulseIgnoredValue(double impulseIgnoredValue) const
{
    return impulseIgnoredValue + m_alignmentAdjustment;
}

double RepeatedScrollSnapPoint::DetermineFirstRepeatedSnapPointValue() const
{
    double actualOffset = ActualOffset();
    double actualStart = ActualStart();

    MUX_ASSERT(actualOffset >= actualStart);
    MUX_ASSERT(m_interval > 0.0);

    return actualOffset - std::floor((actualOffset - actualStart) / m_interval) * m_interval;
}

double RepeatedScrollSnapPoint::DetermineLastRepeatedSnapPointValue() const
{
    double actualOffset = ActualOffset();
    double actualEnd = ActualEnd();

    MUX_ASSERT(actualOffset <= m_end);
    MUX_ASSERT(m_interval > 0.0);

    return actualOffset + std::floor((actualEnd - actualOffset) / m_interval) * m_interval;
}

double RepeatedScrollSnapPoint::DetermineMinActualApplicableZone(
    SnapPointBase* previousSnapPoint) const
{
    double actualStart = ActualStart();

    // The Influence() method of repeated snap points has a check to ensure the value does not fall within its range.
    // This call will ensure that we are not in the range of the previous snap point if it is.
    if (previousSnapPoint)
    {
        previousSnapPoint->Influence(actualStart);
    }
    return actualStart;
}

double RepeatedScrollSnapPoint::DetermineMinActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue) const
{
    if (previousSnapPoint)
    {
        if (currentIgnoredValue == DetermineFirstRepeatedSnapPointValue())
        {
            return currentIgnoredValue;
        }

        if (!isnan(previousIgnoredValue))
        {
            return previousSnapPoint->ImpulseInfluence(ActualStart(), previousIgnoredValue);
        }
    }
    return ActualStart();
}

double RepeatedScrollSnapPoint::DetermineMaxActualApplicableZone(
    SnapPointBase* nextSnapPoint) const
{
    double actualEnd = ActualEnd();

    // The Influence() method of repeated snap points has a check to ensure the value does not fall within its range.
    // This call will ensure that we are not in the range of the next snap point if it is.
    if (nextSnapPoint)
    {
        nextSnapPoint->Influence(actualEnd);
    }
    return actualEnd;
}

double RepeatedScrollSnapPoint::DetermineMaxActualImpulseApplicableZone(
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double nextIgnoredValue) const
{
    if (nextSnapPoint)
    {
        if (currentIgnoredValue == DetermineLastRepeatedSnapPointValue())
        {
            return currentIgnoredValue;
        }

        if (!isnan(nextIgnoredValue))
        {
            return nextSnapPoint->ImpulseInfluence(ActualEnd(), nextIgnoredValue);
        }
    }
    return ActualEnd();
}

void RepeatedScrollSnapPoint::ValidateConstructorParameters(
#ifdef ApplicableRangeType
    bool applicableRangeToo,
    double applicableRange,
#endif
    double offset,
    double interval,
    double start,
    double end) const
{
    if (end <= start)
    {
        throw winrt::hresult_invalid_argument(L"'end' must be greater than 'start'.");
    }

    if (offset < start)
    {
        throw winrt::hresult_invalid_argument(L"'offset' must be greater than or equal to 'start'.");
    }

    if (offset > end)
    {
        throw winrt::hresult_invalid_argument(L"'offset' must be smaller than or equal to 'end'.");
    }

    if (interval <= 0)
    {
        throw winrt::hresult_invalid_argument(L"'interval' must be strictly positive.");
    }

#ifdef ApplicableRangeType
    if (applicableRangeToo && applicableRange <= 0)
    {
        throw winrt::hresult_invalid_argument(L"'applicableRange' must be strictly positive.");
    }
#endif
}

double RepeatedScrollSnapPoint::Influence(double edgeOfMidpoint) const
{
    double actualStart = ActualStart();
    double actualEnd = ActualEnd();

    if (edgeOfMidpoint <= actualStart)
    {
        return actualStart;
    }
    else if (edgeOfMidpoint >= actualEnd)
    {
        return actualEnd;
    }
    else
    {
        // Snap points are not allowed within the bounds (Start thru End) of repeated snap points
        // TODO: Provide custom error message
        throw winrt::hresult_error(E_INVALIDARG);
    }
    return 0.0;
}

double RepeatedScrollSnapPoint::ImpulseInfluence(double edgeOfMidpoint, double ignoredValue) const
{
    if (edgeOfMidpoint <= ActualStart())
    {
        if (ignoredValue == DetermineFirstRepeatedSnapPointValue())
        {
            return ignoredValue;
        }
        return ActualStart();
    }
    else if (edgeOfMidpoint >= ActualEnd())
    {
        if (ignoredValue == DetermineLastRepeatedSnapPointValue())
        {
            return ignoredValue;
        }
        return ActualEnd();
    }
    else
    {
        MUX_ASSERT(false);
        return 0.0;
    }
}

void RepeatedScrollSnapPoint::Combine(
    int& combinationCount,
    winrt::SnapPointBase const& snapPoint) const
{
    // Snap points are not allowed within the bounds (Start thru End) of repeated snap points
    // TODO: Provide custom error message
    throw winrt::hresult_error(E_INVALIDARG);
}

int RepeatedScrollSnapPoint::SnapCount() const
{
    return static_cast<int>((m_end - m_start) / m_interval);
}

double RepeatedScrollSnapPoint::Evaluate(
    std::tuple<double, double> actualApplicableZone,
    double value) const
{
    if (value >= ActualStart() && value <= ActualEnd())
    {
        double firstSnapPointValue = DetermineFirstRepeatedSnapPointValue();
        double passedSnapPoints = std::floor((value - firstSnapPointValue) / m_interval);
        double previousSnapPointValue = (passedSnapPoints * m_interval) + firstSnapPointValue;
        double nextSnapPointValue = previousSnapPointValue + m_interval;

        if ((value - previousSnapPointValue) <= (nextSnapPointValue - value))
        {
            if (previousSnapPointValue + m_specifiedApplicableRange >= value)
            {
                return previousSnapPointValue;
            }
        }
        else
        {
            if (nextSnapPointValue - m_specifiedApplicableRange <= value)
            {
                return nextSnapPointValue;
            }
        }
    }
    return value;
}

/////////////////////////////////////////////////////////////////////
/////////////////       Zoom Snap Points      ///////////////////////
/////////////////////////////////////////////////////////////////////

// Required for Modern Idl bug, should never be called.
ZoomSnapPointBase::ZoomSnapPointBase()
{
    // throw (ERROR_CALL_NOT_IMPLEMENTED);
}

bool ZoomSnapPointBase::OnUpdateViewport(double newViewport)
{
    return false;
}

/////////////////////////////////////////////////////////////////////
//////////////     Irregular Zoom Snap Points    ////////////////////
/////////////////////////////////////////////////////////////////////
CppWinRTActivatableClassWithBasicFactory(ZoomSnapPoint);

ZoomSnapPoint::ZoomSnapPoint(
    double snapPointValue)
{
    m_value = snapPointValue;
}

#ifdef ApplicableRangeType
ZoomSnapPoint::ZoomSnapPoint(
    double snapPointValue,
    double applicableRange)
{
    if (applicableRange <= 0)
    {
        throw winrt::hresult_invalid_argument(L"'applicableRange' must be strictly positive.");
    }

    m_value = snapPointValue;
    m_specifiedApplicableRange = applicableRange;
    m_actualApplicableZone = std::tuple<double, double>{ snapPointValue - applicableRange, snapPointValue + applicableRange };
    m_applicableRangeType = winrt::SnapPointApplicableRangeType::Optional;
}
#endif

double ZoomSnapPoint::Value()
{
    return m_value;
}

// For zoom snap points scale == L"1.0".
// For zoom snap points scale == L"1.0".
winrt::ExpressionAnimation ZoomSnapPoint::CreateRestingPointExpression(
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    winrt::ExpressionAnimation restingPointExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(L"snapPointValue");

    restingPointExpressionAnimation.SetScalarParameter(L"snapPointValue", static_cast<float>(m_value));
    return restingPointExpressionAnimation;
}

// For zoom snap points scale == L"1.0".
winrt::ExpressionAnimation ZoomSnapPoint::CreateConditionalExpression(
    std::tuple<double, double> actualApplicableZone,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    winrt::hstring targetExpression = GetTargetExpression(target);
    winrt::hstring expression = StringUtil::FormatString(
        L"this.Target.IsInertiaFromImpulse ? (%1!s! >= minImpulseApplicableValue && %1!s! <= maxImpulseApplicableValue) : (%1!s! >= minApplicableValue && %1!s! <= maxApplicableValue)",
        targetExpression.data());
    winrt::ExpressionAnimation conditionExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    conditionExpressionAnimation.SetScalarParameter(L"minApplicableValue", static_cast<float>(std::get<0>(actualApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"maxApplicableValue", static_cast<float>(std::get<1>(actualApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"minImpulseApplicableValue", static_cast<float>(std::get<0>(actualImpulseApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"maxImpulseApplicableValue", static_cast<float>(std::get<1>(actualImpulseApplicableZone)));
    return conditionExpressionAnimation;
}

void ZoomSnapPoint::UpdateConditionalExpressionAnimationForImpulse(
    winrt::ExpressionAnimation conditionExpressionAnimation,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
}

void ZoomSnapPoint::UpdateRestingPointExpressionAnimationForImpulse(
    winrt::ExpressionAnimation restingValueExpressionAnimation,
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
}

ScrollerSnapPointSortPredicate ZoomSnapPoint::SortPredicate()
{
    // Irregular snap point should be sorted before repeated snap points so it gives a tertiary sort value of 0 (repeated snap points get 1)
    return ScrollerSnapPointSortPredicate{ m_value, m_value, 0 };
}

std::tuple<double, double> ZoomSnapPoint::DetermineActualApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint)
{
    return std::tuple<double, double>{
        DetermineMinActualApplicableZone(previousSnapPoint),
        DetermineMaxActualApplicableZone(nextSnapPoint) };
}

std::tuple<double, double> ZoomSnapPoint::DetermineActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue,
    double nextIgnoredValue)
{
    return std::tuple<double, double>{
        DetermineMinActualImpulseApplicableZone(
            previousSnapPoint,
            currentIgnoredValue,
            previousIgnoredValue),
        DetermineMaxActualImpulseApplicableZone(
            nextSnapPoint,
            currentIgnoredValue,
            nextIgnoredValue) };
}

double ZoomSnapPoint::DetermineMinActualApplicableZone(
    SnapPointBase* previousSnapPoint) const
{
    // If we are not passed a previousSnapPoint it means we are the first in the list, see if we expand to negative Infinity or stay put.
    if (!previousSnapPoint)
    {
#ifdef ApplicableRangeType
        if (applicableRangeType != winrt::SnapPointApplicableRangeType::Optional)
        {
            return -INFINITY;
        }
        else
        {
            return m_value - m_specifiedApplicableRange;
        }
#else
        return -INFINITY;
#endif
    }
    // If we are passed a previousSnapPoint then we need to account for its influence on us.
    else
    {
        double previousMaxInfluence = previousSnapPoint->Influence(m_value);

#ifdef ApplicableRangeType
        switch (m_applicableRangeType)
        {
        case winrt::SnapPointApplicableRangeType::Optional:
            return std::max(previousMaxInfluence, m_value - m_specifiedApplicableRange);
        case winrt::SnapPointApplicableRangeType::Mandatory:
            return previousMaxInfluence;
        default:
            MUX_ASSERT(false);
            return 0.0;
        }
#else
        return previousMaxInfluence;
#endif
    }
}

double ZoomSnapPoint::DetermineMinActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue) const
{
    if (!previousSnapPoint)
    {
        return -INFINITY;
    }
    else
    {
        double previousMaxInfluence = previousSnapPoint->ImpulseInfluence(m_value, previousIgnoredValue);

        if (isnan(currentIgnoredValue))
        {
            return previousMaxInfluence;
        }
        else
        {
            return std::max(previousMaxInfluence, m_value);
        }
    }
}

double ZoomSnapPoint::DetermineMaxActualApplicableZone(
    SnapPointBase* nextSnapPoint) const
{
    // If we are not passed a nextSnapPoint it means we are the last in the list, see if we expand to Infinity or stay put.
    if (!nextSnapPoint)
    {
#ifdef ApplicableRangeType
        if (m_applicableRangeType != winrt::SnapPointApplicableRangeType::Optional)
        {
            return INFINITY;
        }
        else
        {
            return m_value + m_specifiedApplicableRange;
        }
#else
        return INFINITY;
#endif
    }
    // If we are passed a nextSnapPoint then we need to account for its influence on us.
    else
    {
        double nextMinInfluence = nextSnapPoint->Influence(m_value);

#ifdef ApplicableRangeType
        switch (m_applicableRangeType)
        {
        case winrt::SnapPointApplicableRangeType::Optional:
            return std::min(m_value + m_specifiedApplicableRange, nextMinInfluence);
        case winrt::SnapPointApplicableRangeType::Mandatory:
            return nextMinInfluence;
        default:
            MUX_ASSERT(false);
            return 0.0;
        }
#else
        return nextMinInfluence;
#endif
    }
}

double ZoomSnapPoint::DetermineMaxActualImpulseApplicableZone(
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double nextIgnoredValue) const
{
    if (!nextSnapPoint)
    {
        return INFINITY;
    }
    else
    {
        double nextMinInfluence = nextSnapPoint->ImpulseInfluence(m_value, nextIgnoredValue);

        if (isnan(currentIgnoredValue))
        {
            return nextMinInfluence;
        }
        else
        {
            return std::min(m_value, nextMinInfluence);
        }
    }
}

double ZoomSnapPoint::Influence(double edgeOfMidpoint) const
{
    double midPoint = (m_value + edgeOfMidpoint) / 2;

#ifdef ApplicableRangeType
    switch (m_applicableRangeType)
    {
    case winrt::SnapPointApplicableRangeType::Optional:
        if (m_value <= edgeOfMidpoint)
        {
            return std::min(m_value + m_specifiedApplicableRange, midPoint);
        }
        else
        {
            return std::max(m_value - m_specifiedApplicableRange, midPoint);
        }
    case winrt::SnapPointApplicableRangeType::Mandatory:
        return midPoint;
    default:
        MUX_ASSERT(false);
        return 0.0;
    }
#else
    return midPoint;
#endif
}

double ZoomSnapPoint::ImpulseInfluence(double edgeOfMidpoint, double ignoredValue) const
{
    double midPoint = (m_value + edgeOfMidpoint) / 2.0;

    if (isnan(ignoredValue))
    {
        return midPoint;
    }
    else
    {
        if (m_value <= edgeOfMidpoint)
        {
            return std::min(m_value, midPoint);
        }
        else
        {
            return std::max(m_value, midPoint);
        }
    }
}

void ZoomSnapPoint::Combine(
    int& combinationCount,
    winrt::SnapPointBase const& snapPoint) const
{
    auto snapPointAsIrregular = snapPoint.try_as<winrt::ZoomSnapPoint>();
    if (snapPointAsIrregular)
    {
#ifdef ApplicableRangeType
        //TODO: The m_specifiedApplicableRange field is never expected to change after creation. A correction will be needed here.
        m_specifiedApplicableRange = std::max(snapPointAsIrregular.ApplicableRange(), m_specifiedApplicableRange);
#else
        MUX_ASSERT(m_specifiedApplicableRange == INFINITY);
#endif
        combinationCount++;
    }
    else
    {
        // TODO: Provide custom error message
        throw winrt::hresult_error(E_INVALIDARG);
    }
}

int ZoomSnapPoint::SnapCount() const
{
    return 1;
}

double ZoomSnapPoint::Evaluate(
    std::tuple<double, double> actualApplicableZone,
    double value) const
{
    if (value >= std::get<0>(actualApplicableZone) && value <= std::get<1>(actualApplicableZone))
    {
        return m_value;
    }
    return value;
}

/////////////////////////////////////////////////////////////////////
/////////////////    Repeated Snap Points    ////////////////////////
/////////////////////////////////////////////////////////////////////
CppWinRTActivatableClassWithBasicFactory(RepeatedZoomSnapPoint);

RepeatedZoomSnapPoint::RepeatedZoomSnapPoint(
    double offset,
    double interval,
    double start,
    double end)
{
    ValidateConstructorParameters(
#ifdef ApplicableRangeType
        false /*applicableRangeToo*/,
        0 /*applicableRange*/,
#endif
        offset,
        interval,
        start,
        end);

    m_offset = offset;
    m_interval = interval;
    m_start = start;
    m_end = end;
}

#ifdef ApplicableRangeType
RepeatedZoomSnapPoint::RepeatedZoomSnapPoint(
    double offset,
    double interval,
    double start,
    double end,
    double applicableRange)
{
    ValidateConstructorParameters(
        true /*applicableRangeToo*/,
        applicableRange,
        offset,
        interval,
        start,
        end);

    m_offset = offset;
    m_interval = interval;
    m_start = start;
    m_end = end;
    m_specifiedApplicableRange = applicableRange;
    m_applicableRangeType = winrt::SnapPointApplicableRangeType::Optional;
}
#endif

double RepeatedZoomSnapPoint::Offset()
{
    return m_offset;
}

double RepeatedZoomSnapPoint::Interval()
{
    return m_interval;
}

double RepeatedZoomSnapPoint::Start()
{
    return m_start;
}

double RepeatedZoomSnapPoint::End()
{
    return m_end;
}

// For zoom snap points scale == L"1.0".
winrt::ExpressionAnimation RepeatedZoomSnapPoint::CreateRestingPointExpression(
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    /*
    fracTarget = (target - first) / interval               // Unsnapped value in fractional intervals from first snapping value
    prevSnap = ((Floor(fracTarget) * interval) + first)    // First snapped value before unsnapped value
    nextSnap = ((Ceil(fracTarget) * interval) + first)     // First snapped value after unsnapped value
    effectiveEnd = (IsInertiaFromImpulse ? impEnd : end)   // Regular or impulse upper bound of applicable zone

    Expression:
     ((Abs(target - prevSnap) >= Abs(target - nextSnap)) && (nextSnap <= effectiveEnd))
     ?
     // nextSnap value is closer to unsnapped value and within applicable zone.
     (
      IsInertiaFromImpulse
      ?
      // Impulse mode.
      (
       nextSnap == impIgn
       ?
       (
        // Next snapped value is ignored. Pick the previous snapped value if any, else the ignored value.
        (impIgn == first ? first : impIgn - interval)
       )
       :
       // Pick next snapped value.
       nextSnap
      )
      :
      // Regular mode. Pick next snapped value.
      nextSnap
     )
     :
     // prevSnap value is closer to unsnapped value.
     (
      IsInertiaFromImpulse
      ?
      // Impulse mode.
      (
       prevSnap == impIgn
       ?
       // Previous snapped value is ignored. Pick the next snapped value if any, else the ignored value.
       (impIgn + interval <= effectiveEnd ? impIgn + interval : impIgn)
       :
       // Pick previous snapped value.
       prevSnap
      )
      :
      // Regular mode. Pick previous snapped value.
      prevSnap
     )
    */

    winrt::hstring expression = StringUtil::FormatString(
        L"((Abs(it.%1!s!-((Floor((it.%1!s!-fst)/itv)*itv)+fst))>=Abs(it.%1!s!-((Ceil((it.%1!s!-fst)/itv)*itv)+fst)))&&(((Ceil((it.%1!s!-fst)/itv)*itv)+fst)<=(it.IsInertiaFromImpulse?iEnd:end)))?(it.IsInertiaFromImpulse?(((Ceil((it.%1!s!-fst)/itv)*itv)+fst)==iIgn?((iIgn==fst?fst:iIgn-itv)):(Ceil((it.%1!s!-fst)/itv)*itv)+fst):(Ceil((it.%1!s!-fst)/itv)*itv)+fst):(it.IsInertiaFromImpulse?(((Floor((it.%1!s!-fst)/itv)*itv)+fst)==iIgn?(iIgn+itv<=(it.IsInertiaFromImpulse?iEnd:end)?iIgn+itv:iIgn):(Floor((it.%1!s!-fst)/itv)*itv)+fst):(Floor((it.%1!s!-fst)/itv)*itv)+fst)",
        target.data());

    winrt::ExpressionAnimation restingPointExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    restingPointExpressionAnimation.SetScalarParameter(L"itv", static_cast<float>(m_interval));
    restingPointExpressionAnimation.SetScalarParameter(L"end", static_cast<float>(m_end));
    restingPointExpressionAnimation.SetScalarParameter(L"fst", static_cast<float>(DetermineFirstRepeatedSnapPointValue()));
    restingPointExpressionAnimation.SetReferenceParameter(L"it", interactionTracker);

    UpdateRestingPointExpressionAnimationForImpulse(
        restingPointExpressionAnimation,
        ignoredValue,
        actualImpulseApplicableZone);

    return restingPointExpressionAnimation;
}

// For zoom snap points scale == L"1.0".
winrt::ExpressionAnimation RepeatedZoomSnapPoint::CreateConditionalExpression(
    std::tuple<double, double> actualApplicableZone,
    std::tuple<double, double> actualImpulseApplicableZone,
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    MUX_ASSERT(std::get<0>(actualApplicableZone) == m_start);
    MUX_ASSERT(std::get<1>(actualApplicableZone) == m_end);

    /*
    fracTarget = (target - first) / interval               // Unsnapped value in fractional intervals from first snapping value
    prevSnap = ((Floor(fracTarget) * interval) + first)    // First snapped value before unsnapped value
    nextSnap = ((Ceil(fracTarget) * interval) + first)     // First snapped value after unsnapped value
    effectiveEnd = (IsInertiaFromImpulse ? impEnd : end)   // Regular or impulse upper bound of applicable zone

    Expression:
    (
     (!IsInertiaFromImpulse && target >= start && target <= end)       // If we are within the start and end in non-impulse mode
     ||
     (IsInertiaFromImpulse && target >= impStart && target <= impEnd)  // or we are within the impulse start and end in impulse mode
    )
    &&                                                                 // and...
    (                                                                  // The location of the repeated snap point just before the natural resting point
     (prevSnap + appRange >= target)                                   // Plus the applicable range is greater than the natural resting point
     ||                                                                // or...
     (                                                                 // The location of the repeated snap point just after the natural resting point
      (nextSnap - appRange <= target) &&                               // Minus the applicable range is less than the natural resting point.
      (nextSnap <= effectiveEnd)                                       // And the snap point after the natural resting point is less than or equal to the effective end value
     )
    )
    */

    winrt::hstring expression = StringUtil::FormatString(
        L"((!it.IsInertiaFromImpulse && it.%1!s! >= stt && it.%1!s! <= end) || (it.IsInertiaFromImpulse && it.%1!s! >= iStt && it.%1!s! <= iEnd)) && (((Floor((it.%1!s! - fst) / itv) * itv) + fst + aRg >= it.%1!s!) || (((Ceil((it.%1!s! - fst) / itv) * itv) + fst - aRg <= it.%1!s!) && ((Ceil((it.%1!s! - fst) / itv) * itv) + fst <= (it.IsInertiaFromImpulse ? iEnd : end))))",
        target.data());
    winrt::ExpressionAnimation conditionExpressionAnimation = interactionTracker.Compositor().CreateExpressionAnimation(expression);

    conditionExpressionAnimation.SetScalarParameter(L"itv", static_cast<float>(m_interval));
    conditionExpressionAnimation.SetScalarParameter(L"fst", static_cast<float>(DetermineFirstRepeatedSnapPointValue()));
    conditionExpressionAnimation.SetScalarParameter(L"stt", static_cast<float>(m_start));
    conditionExpressionAnimation.SetScalarParameter(L"end", static_cast<float>(m_end));
    conditionExpressionAnimation.SetScalarParameter(L"aRg", static_cast<float>(m_specifiedApplicableRange));
    conditionExpressionAnimation.SetReferenceParameter(L"it", interactionTracker);

    UpdateConditionalExpressionAnimationForImpulse(
        conditionExpressionAnimation,
        actualImpulseApplicableZone);

    return conditionExpressionAnimation;
}

void RepeatedZoomSnapPoint::UpdateConditionalExpressionAnimationForImpulse(
    winrt::ExpressionAnimation conditionExpressionAnimation,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
    conditionExpressionAnimation.SetScalarParameter(L"iStt", static_cast<float>(std::get<0>(actualImpulseApplicableZone)));
    conditionExpressionAnimation.SetScalarParameter(L"iEnd", static_cast<float>(std::get<1>(actualImpulseApplicableZone)));
}

void RepeatedZoomSnapPoint::UpdateRestingPointExpressionAnimationForImpulse(
    winrt::ExpressionAnimation restingValueExpressionAnimation,
    double ignoredValue,
    std::tuple<double, double> actualImpulseApplicableZone) const
{
    restingValueExpressionAnimation.SetScalarParameter(L"iEnd", static_cast<float>(std::get<1>(actualImpulseApplicableZone)));
    restingValueExpressionAnimation.SetScalarParameter(L"iIgn", static_cast<float>(ignoredValue));
}

ScrollerSnapPointSortPredicate RepeatedZoomSnapPoint::SortPredicate()
{
    // Repeated snap points should be sorted after irregular snap points, so give it a tertiary sort value of 1 (irregular snap points get 0)
    return ScrollerSnapPointSortPredicate{ m_start, m_end, 1 };
}

std::tuple<double, double> RepeatedZoomSnapPoint::DetermineActualApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint)
{
    std::tuple<double, double> actualApplicableZoneReturned = std::tuple<double, double>{
        DetermineMinActualApplicableZone(previousSnapPoint),
        DetermineMaxActualApplicableZone(nextSnapPoint) };

    // Influence() will not have thrown if either of the adjacent snap points are also repeated snap points which have the same start and end, however this is not allowed.
    // We only need to check the nextSnapPoint because of the symmetry in the algorithm.
    if (nextSnapPoint && *static_cast<SnapPointBase*>(this) == (nextSnapPoint))
    {
        // TODO: Provide custom error message
        throw winrt::hresult_error(E_INVALIDARG);
    }

    return actualApplicableZoneReturned;
}

std::tuple<double, double> RepeatedZoomSnapPoint::DetermineActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue,
    double nextIgnoredValue)
{
    return std::tuple<double, double>{
        DetermineMinActualImpulseApplicableZone(
            previousSnapPoint,
            currentIgnoredValue,
            previousIgnoredValue),
        DetermineMaxActualImpulseApplicableZone(
            nextSnapPoint,
            currentIgnoredValue,
            nextIgnoredValue) };
}

double RepeatedZoomSnapPoint::DetermineFirstRepeatedSnapPointValue() const
{
    MUX_ASSERT(m_offset >= m_start);
    MUX_ASSERT(m_interval > 0.0);

    return m_offset - std::floor((m_offset - m_start) / m_interval) * m_interval;
}

double RepeatedZoomSnapPoint::DetermineLastRepeatedSnapPointValue() const
{
    MUX_ASSERT(m_offset <= m_end);
    MUX_ASSERT(m_interval > 0.0);

    return m_offset + std::floor((m_end - m_offset) / m_interval) * m_interval;
}

double RepeatedZoomSnapPoint::DetermineMinActualApplicableZone(
    SnapPointBase* previousSnapPoint) const
{
    // The Influence() method of repeated snap points has a check to ensure the value does not fall within its range.
    // This call will ensure that we are not in the range of the previous snap point if it is.
    if (previousSnapPoint)
    {
        previousSnapPoint->Influence(m_start);
    }
    return m_start;
}

double RepeatedZoomSnapPoint::DetermineMinActualImpulseApplicableZone(
    SnapPointBase* previousSnapPoint,
    double currentIgnoredValue,
    double previousIgnoredValue) const
{
    if (previousSnapPoint)
    {
        if (currentIgnoredValue == DetermineFirstRepeatedSnapPointValue())
        {
            return currentIgnoredValue;
        }

        if (!isnan(previousIgnoredValue))
        {
            return previousSnapPoint->ImpulseInfluence(m_start, previousIgnoredValue);
        }
    }
    return m_start;
}

double RepeatedZoomSnapPoint::DetermineMaxActualApplicableZone(
    SnapPointBase* nextSnapPoint) const
{
    // The Influence() method of repeated snap points has a check to ensure the value does not fall within its range.
    // This call will ensure that we are not in the range of the next snap point if it is.
    if (nextSnapPoint)
    {
        nextSnapPoint->Influence(m_end);
    }
    return m_end;
}

double RepeatedZoomSnapPoint::DetermineMaxActualImpulseApplicableZone(
    SnapPointBase* nextSnapPoint,
    double currentIgnoredValue,
    double nextIgnoredValue) const
{
    if (nextSnapPoint)
    {
        if (currentIgnoredValue == DetermineLastRepeatedSnapPointValue())
        {
            return currentIgnoredValue;
        }

        if (!isnan(nextIgnoredValue))
        {
            return nextSnapPoint->ImpulseInfluence(m_end, nextIgnoredValue);
        }
    }
    return m_end;
}

void RepeatedZoomSnapPoint::ValidateConstructorParameters(
#ifdef ApplicableRangeType
    bool applicableRangeToo,
    double applicableRange,
#endif
    double offset,
    double interval,
    double start,
    double end) const
{
    if (end <= start)
    {
        throw winrt::hresult_invalid_argument(L"'end' must be greater than 'start'.");
    }

    if (offset < start)
    {
        throw winrt::hresult_invalid_argument(L"'offset' must be greater than or equal to 'start'.");
    }

    if (offset > end)
    {
        throw winrt::hresult_invalid_argument(L"'offset' must be smaller than or equal to 'end'.");
    }

    if (interval <= 0)
    {
        throw winrt::hresult_invalid_argument(L"'interval' must be strictly positive.");
    }

#ifdef ApplicableRangeType
    if (applicableRangeToo && applicableRange <= 0)
    {
        throw winrt::hresult_invalid_argument(L"'applicableRange' must be strictly positive.");
    }
#endif
}

double RepeatedZoomSnapPoint::Influence(double edgeOfMidpoint) const
{
    if (edgeOfMidpoint <= m_start)
    {
        return m_start;
    }
    else if (edgeOfMidpoint >= m_end)
    {
        return m_end;
    }
    else
    {
        // Snap points are not allowed within the bounds (Start thru End) of repeated snap points
        // TODO: Provide custom error message
        throw winrt::hresult_error(E_INVALIDARG);
    }
    return 0.0;
}

double RepeatedZoomSnapPoint::ImpulseInfluence(double edgeOfMidpoint, double ignoredValue) const
{
    if (edgeOfMidpoint <= m_start)
    {
        if (ignoredValue == DetermineFirstRepeatedSnapPointValue())
        {
            return ignoredValue;
        }
        return m_start;
    }
    else if (edgeOfMidpoint >= m_end)
    {
        if (ignoredValue == DetermineLastRepeatedSnapPointValue())
        {
            return ignoredValue;
        }
        return m_end;
    }
    else
    {
        MUX_ASSERT(false);
        return 0.0;
    }
}

void RepeatedZoomSnapPoint::Combine(
    int& combinationCount,
    winrt::SnapPointBase const& snapPoint) const
{
    // Snap points are not allowed within the bounds (Start thru End) of repeated snap points
    // TODO: Provide custom error message
    throw winrt::hresult_error(E_INVALIDARG);
}

int RepeatedZoomSnapPoint::SnapCount() const
{
    return static_cast<int>((m_end - m_start) / m_interval);
}

double RepeatedZoomSnapPoint::Evaluate(
    std::tuple<double, double> actualApplicableZone,
    double value) const
{
    if (value >= m_start && value <= m_end)
    {
        double firstSnapPointValue = DetermineFirstRepeatedSnapPointValue();
        double passedSnapPoints = std::floor((value - firstSnapPointValue) / m_interval);
        double previousSnapPointValue = (passedSnapPoints * m_interval) + firstSnapPointValue;
        double nextSnapPointValue = previousSnapPointValue + m_interval;

        if ((value - previousSnapPointValue) <= (nextSnapPointValue - value))
        {
            if (previousSnapPointValue + m_specifiedApplicableRange >= value)
            {
                return previousSnapPointValue;
            }
        }
        else
        {
            if (nextSnapPointValue - m_specifiedApplicableRange <= value)
            {
                return nextSnapPointValue;
            }
        }
    }
    return value;
}
