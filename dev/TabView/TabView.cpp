﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "common.h"
#include "TabView.h"
#include "DoubleUtil.h"
#include "RuntimeProfiler.h"
#include "ResourceAccessor.h"
#include "SharedHelpers.h"

static constexpr double c_tabMaximumWidth = 200.0;
static constexpr double c_tabMinimumWidth = 48.0;

TabView::TabView()
{
    __RP_Marker_ClassById(RuntimeProfiler::ProfId_TabView);

    SetDefaultStyleKey(this);
}

void TabView::OnApplyTemplate()
{
    winrt::IControlProtected controlProtected{ *this };

    m_tabContentPresenter.set(GetTemplateChildT<winrt::ContentPresenter>(L"TabContentPresenter", controlProtected));
    m_scrollViewer.set(GetTemplateChildT<winrt::FxScrollViewer>(L"ScrollViewer", controlProtected));

    //### do I need a revoker when listening to my own event....??
    m_loadedRevoker = Loaded(winrt::auto_revoke, { this, &TabView::OnLoaded });
    m_selectionChangedRevoker = SelectionChanged(winrt::auto_revoke, { this, &TabView::OnSelectionChanged });
    m_sizeChangedRevoker = SizeChanged(winrt::auto_revoke, { this, &TabView::OnSizeChanged });
}

void TabView::OnPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args)
{
    winrt::IDependencyProperty property = args.Property();

    if (property == s_TabWidthModeProperty)
    {
        UpdateTabWidths();
    }
}

void TabView::OnLoaded(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args)
{
    winrt::IControlProtected controlProtected{ *this };

    // ### yeah probably need to do this on scrollviewer load, not this on loaded, anyway.
    if (auto scrollViewer = m_scrollViewer.get())
    {
        m_scrollDecreaseButton.set(SharedHelpers::FindInVisualTreeByName(scrollViewer, L"ScrollDecreaseButton").as<winrt::RepeatButton>());
        if (auto decreaseButton = m_scrollDecreaseButton.get())
        {
            m_scrollDecreaseClickRevoker = decreaseButton.Click(winrt::auto_revoke, { this, &TabView::OnScrollDecreaseClick });
        }

        m_scrollIncreaseButton.set(SharedHelpers::FindInVisualTreeByName(scrollViewer, L"ScrollIncreaseButton").as<winrt::RepeatButton>());
        if (auto increaseButton = m_scrollIncreaseButton.get())
        {
            m_scrollIncreaseClickRevoker = increaseButton.Click(winrt::auto_revoke, { this, &TabView::OnScrollIncreaseClick });
        }
    }

    UpdateTabWidths();
}

void TabView::OnSizeChanged(const winrt::IInspectable& sender, const winrt::SizeChangedEventArgs& args)
{
    UpdateTabWidths();
}

void TabView::OnItemsChanged(winrt::IInspectable const& item)
{
    if (auto args = item.as< winrt::IVectorChangedEventArgs>())
    {
        if (args.CollectionChange() == winrt::CollectionChange::ItemRemoved && Items().Size() > 0)
        {
            if (SelectedIndex() == (int32_t)args.Index())
            {
                int index = (int)args.Index();
                if (index >= (int)Items().Size())
                {
                    index = (int)Items().Size() - 1;
                }

                // ### the item could be disabled, though, you probably need to iterate a bit.
                // ### why this no work SelectedItem(Items().GetAt(index));

                m_isTabClosing = true;
                m_indexToSelect = index;

                /* this didn't help:
                auto container = ContainerFromItem(Items().GetAt(index));
                if (auto lvi = container.as<winrt::ListViewItem>())
                {
                    lvi.IsSelected(true);
                }*/
            }
        }
    }

    UpdateTabWidths();

    __super::OnItemsChanged(item);
}

void TabView::OnSelectionChanged(const winrt::IInspectable& sender, const winrt::SelectionChangedEventArgs& args)
{
    WCHAR strOut[1024];
    StringCchPrintf(strOut, ARRAYSIZE(strOut), L"OnSelectionChanged: selected index %d\n", SelectedIndex());
    OutputDebugString(strOut);

    if (m_isTabClosing)
    {
        m_isTabClosing = false;
        SelectedItem(Items().GetAt(m_indexToSelect));
    }

    if (auto tabContentPresenter = m_tabContentPresenter.get())
    {
        if (!SelectedItem())
        {
            tabContentPresenter.Content(nullptr);
            tabContentPresenter.ContentTemplate(nullptr);
        }
        else
        {
            auto container = ContainerFromItem(SelectedItem()).as<winrt::ListViewItem>();
            if (container)
            {
                //if (ContainerFromItem(SelectedItem) is TabViewItem container)
                //{
                tabContentPresenter.Content(container.Content());
                tabContentPresenter.ContentTemplate(container.ContentTemplate());
                //}
            }
        }
    }
}

void TabView::CloseTab(winrt::TabViewItem container)
{
    // ### going to want to notify the owner I guess and give them a chance to say no, but for now....
    if (auto item = ItemFromContainer(container))
    {
        uint32_t index = 0;
        if (Items().IndexOf(item, index))
        {
            auto args = winrt::make_self<TabViewTabClosingEventArgs>(item);

            m_tabClosingEventSource(*this, *args);

            if (!args->Cancel())
            {
                if (SelectedIndex() == (int32_t)index)
                {
                    // Select a new item.... hey, why doesn't the control seem to actually do that?

                }

                Items().RemoveAt(index);
            }
        }
    }
}

void TabView::OnScrollDecreaseClick(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args)
{
    if (auto scrollViewer = m_scrollViewer.get())
    {
        scrollViewer.ChangeView(std::max(0.0, scrollViewer.HorizontalOffset() - 50.0), nullptr, nullptr); //### magic numbers
    }
}

void TabView::OnScrollIncreaseClick(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args)
{
    if (auto scrollViewer = m_scrollViewer.get())
    {
        scrollViewer.ChangeView(std::min(scrollViewer.ScrollableWidth(), scrollViewer.HorizontalOffset() + 50.0), nullptr, nullptr);
    }
}

void TabView::UpdateTabWidths()
{
    // ### also call this on size changed
    if (TabWidthMode() == winrt::TabViewWidthMode::SizeToContent)
    {
        for (int i = 0; i < (int)(Items().Size()); i++)
        {
            auto container = ContainerFromItem(Items().GetAt(i)).as<winrt::ListViewItem>();
            if (container)
            {
                container.Width(DoubleUtil::NaN);
            }
        }
    }
    else
    {
        //### need function for this -- probably somewhere in common
        double maxTabWidth = c_tabMaximumWidth;
        auto resourceName = box_value(L"TabViewItemMaxWidth");
        if (winrt::Application::Current().Resources().HasKey(resourceName))
        {
            if (auto lookup = winrt::Application::Current().Resources().Lookup(resourceName))
            {
                maxTabWidth = unbox_value<double>(lookup);
            }
        }

        double minTabWidth = c_tabMinimumWidth;
        resourceName = box_value(L"TabViewItemMinWidth");
        if (winrt::Application::Current().Resources().HasKey(resourceName))
        {
            if (auto lookup = winrt::Application::Current().Resources().Lookup(resourceName))
            {
                minTabWidth = unbox_value<double>(lookup);
            }
        }

        double tabWidth = maxTabWidth;
        if (auto scrollViewer = m_scrollViewer.get())
        {
            double padding = Padding().Left + Padding().Right;

            // ### probably some rounding error here....?
            double tabWidthForScroller = (scrollViewer.ActualWidth() - padding) / (double)(Items().Size()); //### ExtentWidth() didn't work??
            tabWidth = std::min(std::max(tabWidthForScroller, minTabWidth), maxTabWidth);

            auto decreaseButton = m_scrollDecreaseButton.get();
            auto increaseButton = m_scrollIncreaseButton.get();
            if (decreaseButton && increaseButton)
            {
                // ### maybe this is right, maybe there's something else to check...
                if (tabWidthForScroller < tabWidth)
                {
                    decreaseButton.Visibility(winrt::Visibility::Visible);
                    increaseButton.Visibility(winrt::Visibility::Visible);
                }
                else
                {
                    decreaseButton.Visibility(winrt::Visibility::Collapsed);
                    increaseButton.Visibility(winrt::Visibility::Collapsed);
                }
            }
        }

        for (int i = 0; i < (int)(Items().Size()); i++)
        {
            auto container = ContainerFromItem(Items().GetAt(i)).as<winrt::ListViewItem>();
            if (container)
            {
                container.Width(tabWidth);
            }
        }
    }
}
