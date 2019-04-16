﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "common.h"
#include "SnapPointWrapper.h"

template<typename T>
SnapPointWrapper<T>::SnapPointWrapper(T const& snapPoint)
    : m_snapPoint(snapPoint)
{
}

#ifdef _DEBUG
template<typename T>
SnapPointWrapper<T>::~SnapPointWrapper()
{
}
#endif //_DEBUG

template<typename T>
T SnapPointWrapper<T>::SnapPoint() const
{
    return m_snapPoint;
}

template<typename T>
std::tuple<double, double> SnapPointWrapper<T>::ActualApplicableZone() const
{
    return m_actualApplicableZone;
}

template<typename T>
int SnapPointWrapper<T>::CombinationCount() const
{
    return m_combinationCount;
}

template<typename T>
bool SnapPointWrapper<T>::ResetIgnoredValue()
{
    if (!isnan(m_ignoredValue))
    {
        m_ignoredValue = NAN;
        return true;
    }

    return false;
}

template<typename T>
void SnapPointWrapper<T>::SetIgnoredValue(double ignoredValue)
{
    MUX_ASSERT(!isnan(ignoredValue));

    m_ignoredValue = ignoredValue;
}

template<typename T>
winrt::ExpressionAnimation SnapPointWrapper<T>::CreateRestingPointExpression(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);

    m_restingValueExpressionAnimation = snapPoint->CreateRestingPointExpression(
        m_ignoredValue,
        m_actualImpulseApplicableZone,
        interactionTracker,
        target,
        scale);

    return m_restingValueExpressionAnimation;
}

template<typename T>
winrt::ExpressionAnimation SnapPointWrapper<T>::CreateConditionalExpression(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale)
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);

    m_conditionExpressionAnimation = snapPoint->CreateConditionalExpression(
        m_actualApplicableZone,
        m_actualImpulseApplicableZone,
        interactionTracker,
        target,
        scale);

    return m_conditionExpressionAnimation;
}

template<typename T>
void SnapPointWrapper<T>::GetUpdatedExpressionAnimationsForImpulse(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::ExpressionAnimation* conditionalExpressionAnimation,
    winrt::ExpressionAnimation* restingPointExpressionAnimation)
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);

    snapPoint->UpdateConditionalExpressionAnimationForImpulse(
        m_conditionExpressionAnimation,
        m_actualImpulseApplicableZone);
    snapPoint->UpdateRestingPointExpressionAnimationForImpulse(
        m_restingValueExpressionAnimation,
        m_ignoredValue,
        m_actualImpulseApplicableZone);

    *conditionalExpressionAnimation = m_conditionExpressionAnimation;
    *restingPointExpressionAnimation = m_restingValueExpressionAnimation;
}

template<typename T>
void SnapPointWrapper<T>::DetermineActualApplicableZone(
    SnapPointWrapper<T>* previousSnapPointWrapper,
    SnapPointWrapper<T>* nextSnapPointWrapper,
    bool forImpulseOnly)
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);
    SnapPointBase* previousSnapPoint = GetSnapPointFromWrapper(previousSnapPointWrapper);
    SnapPointBase* nextSnapPoint = GetSnapPointFromWrapper(nextSnapPointWrapper);
    double previousIgnoredValue = previousSnapPointWrapper ? previousSnapPointWrapper->m_ignoredValue : NAN;
    double nextIgnoredValue = nextSnapPointWrapper ? nextSnapPointWrapper->m_ignoredValue : NAN;

    if (!forImpulseOnly)
    {
        m_actualApplicableZone = snapPoint->DetermineActualApplicableZone(
            previousSnapPoint,
            nextSnapPoint);
    }

    m_actualImpulseApplicableZone = snapPoint->DetermineActualImpulseApplicableZone(
        previousSnapPoint,
        nextSnapPoint,
        m_ignoredValue,
        previousIgnoredValue,
        nextIgnoredValue);
}

template<typename T>
void SnapPointWrapper<T>::Combine(SnapPointWrapper<T>* snapPointWrapper)
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);

    snapPoint->Combine(m_combinationCount, snapPointWrapper->SnapPoint());
}

template<typename T>
double SnapPointWrapper<T>::Evaluate(double value) const
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);

    return snapPoint->Evaluate(m_actualApplicableZone, value);
}

template<typename T>
bool SnapPointWrapper<T>::SnapsAt(double value) const
{
    SnapPointBase* snapPoint = GetSnapPointFromWrapper(this);

    return snapPoint->SnapsAt(m_actualApplicableZone, value);
}

template<typename T>
SnapPointBase* SnapPointWrapper<T>::GetSnapPointFromWrapper(std::shared_ptr<SnapPointWrapper<T>> snapPointWrapper)
{
    return GetSnapPointFromWrapper(snapPointWrapper.get());
}

template<typename T>
SnapPointBase* SnapPointWrapper<T>::GetSnapPointFromWrapper(const SnapPointWrapper<T>* snapPointWrapper)
{
    if (snapPointWrapper)
    {
        winrt::SnapPointBase winrtPreviousSnapPoint = safe_cast<winrt::SnapPointBase>(snapPointWrapper->SnapPoint());
        return winrt::get_self<SnapPointBase>(winrtPreviousSnapPoint);
    }
    return nullptr;
}

template SnapPointWrapper<winrt::ScrollSnapPointBase>::SnapPointWrapper(winrt::ScrollSnapPointBase const& snapPoint);
template SnapPointWrapper<winrt::ZoomSnapPointBase>::SnapPointWrapper(winrt::ZoomSnapPointBase const& snapPoint);

#ifdef _DEBUG
template SnapPointWrapper<winrt::ScrollSnapPointBase>::~SnapPointWrapper();
template SnapPointWrapper<winrt::ZoomSnapPointBase>::~SnapPointWrapper();
#endif //_DEBUG

template winrt::ScrollSnapPointBase SnapPointWrapper<winrt::ScrollSnapPointBase>::SnapPoint() const;
template winrt::ZoomSnapPointBase SnapPointWrapper<winrt::ZoomSnapPointBase>::SnapPoint() const;

template std::tuple<double, double> SnapPointWrapper<winrt::ScrollSnapPointBase>::ActualApplicableZone() const;
template std::tuple<double, double> SnapPointWrapper<winrt::ZoomSnapPointBase>::ActualApplicableZone() const;

template int SnapPointWrapper<winrt::ScrollSnapPointBase>::CombinationCount() const;
template int SnapPointWrapper<winrt::ZoomSnapPointBase>::CombinationCount() const;

template bool SnapPointWrapper<winrt::ScrollSnapPointBase>::ResetIgnoredValue();
template bool SnapPointWrapper<winrt::ZoomSnapPointBase>::ResetIgnoredValue();

template void SnapPointWrapper<winrt::ScrollSnapPointBase>::SetIgnoredValue(double ignoredValue);
template void SnapPointWrapper<winrt::ZoomSnapPointBase>::SetIgnoredValue(double ignoredValue);

template winrt::ExpressionAnimation SnapPointWrapper<winrt::ScrollSnapPointBase>::CreateRestingPointExpression(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale);
template winrt::ExpressionAnimation SnapPointWrapper<winrt::ZoomSnapPointBase>::CreateRestingPointExpression(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale);

template winrt::ExpressionAnimation SnapPointWrapper<winrt::ScrollSnapPointBase>::CreateConditionalExpression(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale);
template winrt::ExpressionAnimation SnapPointWrapper<winrt::ZoomSnapPointBase>::CreateConditionalExpression(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::hstring const& scale);

template void SnapPointWrapper<winrt::ScrollSnapPointBase>::GetUpdatedExpressionAnimationsForImpulse(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::ExpressionAnimation* conditionalExpressionAnimation,
    winrt::ExpressionAnimation* restingPointExpressionAnimation);
template void SnapPointWrapper<winrt::ZoomSnapPointBase>::GetUpdatedExpressionAnimationsForImpulse(
    winrt::InteractionTracker const& interactionTracker,
    winrt::hstring const& target,
    winrt::ExpressionAnimation* conditionalExpressionAnimation,
    winrt::ExpressionAnimation* restingPointExpressionAnimation);

template void SnapPointWrapper<winrt::ScrollSnapPointBase>::DetermineActualApplicableZone(
    SnapPointWrapper<winrt::ScrollSnapPointBase>* previousSnapPoint,
    SnapPointWrapper<winrt::ScrollSnapPointBase>* nextSnapPoint,
    bool forImpulseOnly);
template void SnapPointWrapper<winrt::ZoomSnapPointBase>::DetermineActualApplicableZone(
    SnapPointWrapper<winrt::ZoomSnapPointBase>* previousSnapPoint,
    SnapPointWrapper<winrt::ZoomSnapPointBase>* nextSnapPoint,
    bool forImpulseOnly);

template void SnapPointWrapper<winrt::ScrollSnapPointBase>::Combine(SnapPointWrapper<winrt::ScrollSnapPointBase>* snapPointWrapper);
template void SnapPointWrapper<winrt::ZoomSnapPointBase>::Combine(SnapPointWrapper<winrt::ZoomSnapPointBase>* snapPointWrapper);

template double SnapPointWrapper<winrt::ScrollSnapPointBase>::Evaluate(double value) const;
template double SnapPointWrapper<winrt::ZoomSnapPointBase>::Evaluate(double value) const;

template bool SnapPointWrapper<winrt::ScrollSnapPointBase>::SnapsAt(double value) const;
template bool SnapPointWrapper<winrt::ZoomSnapPointBase>::SnapsAt(double value) const;

template SnapPointBase* SnapPointWrapper<winrt::ScrollSnapPointBase>::GetSnapPointFromWrapper(std::shared_ptr<SnapPointWrapper<winrt::ScrollSnapPointBase>> snapPointWrapper);
template SnapPointBase* SnapPointWrapper<winrt::ZoomSnapPointBase>::GetSnapPointFromWrapper(std::shared_ptr<SnapPointWrapper<winrt::ZoomSnapPointBase>> snapPointWrapper);
