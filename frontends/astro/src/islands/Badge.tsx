import { useEffect, useState } from "preact/hooks";

export default function Badge({ count }: { count: number }) {
  const [n, setN] = useState(count);
  useEffect(() => {
    const on = (e: Event) => setN((e as CustomEvent<number>).detail);
    window.addEventListener("cart:count", on);
    return () => window.removeEventListener("cart:count", on);
  }, []);
  return <span class="badge" id="cart-badge" data-testid="cart-badge">{n}</span>;
}
