#pragma once

/**
 * @file CartManager.h
 * @brief In-memory cart storage shared between catalog, cart, and checkout dialogs.
 */
#include "../models/DataModels.h"

/**
 * @brief Stores products selected by the current user before checkout.
 */
class CartManager
{
public:
  /**
   * @brief Adds a product to the cart or increases its quantity.
   * @param p Product to add.
   * @param quantity Quantity to add. Values less than one are ignored.
   */
  void addProduct(const Product &p, int quantity = 1)
  {
    if (quantity <= 0)
      return;

    for (CartItem &item : m_items) {
      if (item.product.id == p.id) {
        item.quantity += quantity;
        return;
      }
    }
    CartItem item;
    item.product = p;
    item.quantity = quantity;
    m_items.push_back(item);
  }

  /**
   * @brief Returns all current cart items.
   */
  const QVector<CartItem>& items() const { return m_items; }

  /**
   * @brief Calculates total cart price.
   */
  double total() const
  {
    double sum = 0.0;
    for (const CartItem &item : m_items) {
      sum += item.product.price * item.quantity;
    }
    return sum;
  }

  /**
   * @brief Removes all products from the cart.
   */
  void clear() { m_items.clear(); }

  /**
   * @brief Removes a product by identifier.
   * @param productId Product identifier.
   */
  void removeProduct(int productId)
  {
    for (int i = 0; i < m_items.size(); ++i) {
      if (m_items[i].product.id == productId) {
        m_items.removeAt(i);
        return;
      }
    }
  }

  /**
   * @brief Sets product quantity or removes the item when quantity is zero or negative.
   * @param productId Product identifier.
   * @param quantity New quantity.
   */
  void setQuantity(int productId, int quantity)
  {
    if (quantity <= 0) {
      removeProduct(productId);
      return;
    }

    for (CartItem &item : m_items) {
      if (item.product.id == productId) {
        item.quantity = quantity;
        return;
      }
    }
  }

  /**
   * @brief Changes product quantity by a relative delta.
   * @param productId Product identifier.
   * @param delta Quantity increment or decrement.
   */
  void changeQuantity(int productId, int delta)
  {
    for (CartItem &item : m_items) {
      if (item.product.id == productId) {
        setQuantity(productId, item.quantity + delta);
        return;
      }
    }
  }

private:
  QVector<CartItem> m_items;
};

