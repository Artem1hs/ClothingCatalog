#pragma once

/**
 * @file DataModels.h
 * @brief Shared data structures used by catalog, cart, profile, and recommendation modules.
 */
#include "../app/AppCommon.h"

/**
 * @brief Product card data loaded from the local API and shown in the catalog.
 */
struct Product
{
  int   id = 0;              ///< Product identifier in the local database.
  QString name;              ///< Display name.
  QString category;          ///< Product category.
  QString color;             ///< Display color.
  QString size;              ///< Clothing or shoe size.
  QString material;          ///< Product material.
  QString season;            ///< Recommended season.
  QString styleTag;          ///< Style tag used by recommendations.
  double price = 0.0;        ///< Product price.
  QString imagePath;         ///< Main relative image path.
  QString imagePath2;        ///< First additional relative image path.
  QString imagePath3;        ///< Second additional relative image path.
};

/**
 * @brief Product entry stored in the cart together with selected quantity.
 */
struct CartItem
{
  Product product;           ///< Product selected by the user.
  int   quantity = 1;        ///< Selected quantity.
};

/**
 * @brief User profile fields used by recommendation dialogs.
 */
struct UserProfile
{
  QString username;          ///< User login.
  QString gender;            ///< Gender preference.
  int   height = 0;          ///< Height in centimeters.
  QString style;             ///< Preferred style.
  QString budget;            ///< Budget text value.
  QString favoriteColors;    ///< Preferred colors.
};


/**
 * @brief Search criteria used to score and filter recommended products.
 */
struct RecommendCriteria
{
  QString gender;            ///< Gender filter.
  int   weight = 0;          ///< Weight value for extended profile matching.
  QString size;              ///< Required size.
  int   height = 0;          ///< Height in centimeters.
  QString style;             ///< Preferred style.
  QString category;          ///< Product category filter.
  QString colors;            ///< Color filter.
  double minBudget = 0.0;    ///< Minimal allowed price.
  double maxBudget = 0.0;    ///< Maximal allowed price.
  QString currency;          ///< Display currency.
};

