import type { CDPSession } from "playwright";

export type Profile = "desktop" | "mobile";
/** desktop: unthrottled. mobile: 4x CPU slowdown, ~1.6 Mbps down / 750 kbps up, 150 ms RTT. */
export async function applyProfile(cdp: CDPSession, profile: Profile) {
  if (profile === "desktop") return;
  await cdp.send("Emulation.setCPUThrottlingRate", { rate: 4 });
  await cdp.send("Network.enable");
  await cdp.send("Network.emulateNetworkConditions", { offline: false, latency: 150, downloadThroughput: 200_000, uploadThroughput: 93_750 });
}
